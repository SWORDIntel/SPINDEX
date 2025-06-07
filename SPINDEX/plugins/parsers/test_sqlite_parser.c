#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h> // For unlink already here, good.

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"
#include "sqlite_parser.h"

// Helper function to create a ProcessedDataChunk from a file's content
ProcessedDataChunk* create_chunk_from_file(const char* filepath, const char* source_id_suffix) {
    LOG_DEBUG("Attempting to open file at path: '%s'", filepath);
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        LOG_ERROR("Test Helper: Failed to open file '%s': %s", filepath, strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        LOG_ERROR("Test Helper: Failed to get file size or file is empty '%s': %s", filepath, strerror(errno));
        fclose(file);
        return NULL;
    }

    unsigned char *buffer = (unsigned char*)malloc(file_size + 1);
    if (!buffer) {
        LOG_ERROR("Test Helper: Failed to allocate memory for file buffer for '%s'", filepath);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        LOG_ERROR("Test Helper: Failed to read full file content from '%s'. Read %zu of %ld bytes. Error: %s", filepath, bytes_read, file_size, strerror(errno));
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[file_size] = '\0';

    ProcessedDataChunk *chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!chunk) {
        LOG_ERROR("Test Helper: Failed to allocate memory for ProcessedDataChunk for '%s'", filepath);
        free(buffer);
        fclose(file);
        return NULL;
    }

    chunk->data = buffer;
    chunk->length = bytes_read;
    chunk->data_type = CHUNK_TYPE_RAW_BYTES;

    char source_id_buf[512];
    snprintf(source_id_buf, sizeof(source_id_buf), "%s%s", filepath, source_id_suffix ? source_id_suffix : "");
    chunk->source_id = strdup(source_id_buf);
    if (!chunk->source_id) {
        LOG_WARN("Test Helper: Failed to allocate memory for source_id for '%s'.", filepath);
    }

    fclose(file);
    LOG_INFO("Test Helper: Successfully created chunk from file '%s', size %zu", filepath, chunk->length);
    return chunk;
}

void free_chunk_data(ProcessedDataChunk *chunk) {
    if (chunk) {
        if (chunk->data) {
            free(chunk->data);
            chunk->data = NULL;
        }
        if (chunk->source_id) {
            free(chunk->source_id);
            chunk->source_id = NULL;
        }
        free(chunk);
    }
}

void test_sqlite_parsing() {
    LOG_INFO("--- Running SQLite Parsing Test ---");
    spindex_plugin_init();

    const char* db_path = "test_data/sample_sqlite_parser_test_from_c.db";
    // Create the DB file using system calls
    char command[1024];
    snprintf(command, sizeof(command), "mkdir -p test_data && rm -f %s && sqlite3 %s \"CREATE TABLE T1 (ID INT PRIMARY KEY NOT NULL, NAME TEXT NOT NULL);\" && sqlite3 %s \"INSERT INTO T1 (ID,NAME) VALUES (1, 'Hello World');\" && sqlite3 %s \"INSERT INTO T1 (ID,NAME) VALUES (2, 'Test Data');\" && sqlite3 %s \"CREATE TABLE T2 (INFO TEXT);\" && sqlite3 %s \"INSERT INTO T2 (INFO) VALUES ('Another table content');\"",
             db_path, db_path, db_path, db_path, db_path, db_path);

    LOG_INFO("Creating sample SQLite DB with command: %s", command);
    int ret = system(command);
    assert(ret == 0 && "Failed to create sample SQLite DB via system call");

    ProcessedDataChunk *input_chunk = create_chunk_from_file(db_path, ".db");
    assert(input_chunk != NULL && "Failed to create input chunk for SQLite test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);
    assert(output_chunk != NULL && "Parser returned NULL for SQLite file");
    assert(output_chunk->data != NULL && "Parser output data is NULL for SQLite file");
    // .dump output can be large, check for key parts
    assert(output_chunk->length > 0 && "Parser output data length is 0 (SQLite)");
    assert(output_chunk->data_type == CHUNK_TYPE_TEXT_PARSED && "Parser output data_type is not CHUNK_TYPE_TEXT_PARSED (SQLite)");

    LOG_INFO("SQLite Parser Output (first 200 bytes): %.*s", 200, (char*)output_chunk->data);
    assert(strstr((char*)output_chunk->data, "CREATE TABLE T1") != NULL && "Phrase 'CREATE TABLE T1' not found in SQLite dump");
    assert(strstr((char*)output_chunk->data, "Hello World") != NULL && "Phrase 'Hello World' not found in SQLite dump");
    assert(strstr((char*)output_chunk->data, "CREATE TABLE T2") != NULL && "Phrase 'CREATE TABLE T2' not found in SQLite dump");
    assert(strstr((char*)output_chunk->data, "Another table content") != NULL && "Phrase 'Another table content' not found in SQLite dump");

    LOG_INFO("SQLite parsing test successful.");
    free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    unlink(db_path); // Clean up the created DB
    spindex_plugin_shutdown();
}

void test_non_sqlite_file_parsing() {
    LOG_INFO("--- Running Non-SQLite File Parsing Test ---");
    spindex_plugin_init();

    // Create a dummy non-SQLite file (e.g., empty text file)
    const char* bad_db_path = "test_data/bad_parser.db"; // Use different name to avoid conflict
    FILE* fp = fopen(bad_db_path, "w");
    assert(fp != NULL && "Failed to create dummy bad_parser.db file");
    fprintf(fp, "This is not an sqlite database.");
    fclose(fp);

    ProcessedDataChunk *input_chunk = create_chunk_from_file(bad_db_path, ".db");
    assert(input_chunk != NULL && "Failed to create input chunk for non-SQLite test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);
    // sqlite3 .dump on a non-db file usually returns non-zero and prints an error to stderr
    // The output file might be empty or not created.
    // The plugin should handle this by returning NULL or an empty chunk.
    if (output_chunk != NULL && output_chunk->length > 0) {
        LOG_WARN("SQLite parser returned non-empty output for a non-SQLite file. This might be an error message from sqlite3 .dump. Output: %s", (char*)output_chunk->data);
    } else if (output_chunk == NULL) {
        LOG_INFO("SQLite parser correctly returned NULL for non-SQLite file input.");
    } else if (output_chunk != NULL && output_chunk->length == 0) {
        LOG_INFO("SQLite parser correctly returned an empty chunk for non-SQLite file input.");
    }
    // Key is that it doesn't crash and handles error from sqlite3.
    // The plugin itself logs "Command execution failed..."

    if(output_chunk) free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    unlink(bad_db_path); // Clean up dummy file
    spindex_plugin_shutdown();
    LOG_INFO("Non-SQLite file parsing test completed.");
}

void test_null_input() {
    LOG_INFO("--- Running NULL Input Test (SQLite Parser) ---");
    spindex_plugin_init();
    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(NULL);
    assert(output_chunk == NULL && "Parser did not return NULL for NULL input chunk");
    LOG_INFO("NULL input chunk test successful.");

    ProcessedDataChunk chunk_no_data;
    memset(&chunk_no_data, 0, sizeof(ProcessedDataChunk));
    chunk_no_data.source_id = strdup("test_null_data.db");
    chunk_no_data.data = NULL;
    chunk_no_data.length = 0;
    chunk_no_data.data_type = CHUNK_TYPE_RAW_BYTES;

    output_chunk = spindex_plugin_process_data(&chunk_no_data);
    assert(output_chunk == NULL && "Parser did not return NULL for chunk with NULL data");
    LOG_INFO("Chunk with NULL data test successful.");

    if (chunk_no_data.source_id) free(chunk_no_data.source_id);
    spindex_plugin_shutdown();
}

int main() {
    LOG_INFO("Starting SQLite Parser tests...");
    PluginInfo info;
    spindex_plugin_get_info(&info);
    LOG_INFO("Plugin Name: %s", info.name);
    LOG_INFO("Plugin Version: %s", info.version);
    LOG_INFO("Plugin Type: %d", info.type);
    LOG_INFO("Plugin Description: %s", info.description);
    assert(strcmp(info.name, "SQLiteParser") == 0);
    assert(strcmp(info.version, "0.2.0") == 0);
    assert(info.type == SPINDEX_PLUGIN_TYPE_PARSER);

    test_sqlite_parsing();
    test_non_sqlite_file_parsing();
    test_null_input();

    LOG_INFO("All SQLite Parser tests completed.");
    return 0;
}
