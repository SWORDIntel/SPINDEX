#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"
#include "sqlite_preprocessor.h"

// Helper function to create a ProcessedDataChunk from a string
ProcessedDataChunk* create_chunk_from_string(const char* text_content, ChunkDataType type, const char* source_id_str) {
    if (!text_content) {
        LOG_ERROR("Test Helper: text_content is NULL.");
        return NULL;
    }
    size_t len = strlen(text_content);
    unsigned char *buffer = (unsigned char*)malloc(len + 1);
    if (!buffer) {
        LOG_ERROR("Test Helper: Failed to allocate memory for text buffer.");
        return NULL;
    }
    strcpy((char*)buffer, text_content);
    ProcessedDataChunk *chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!chunk) {
        LOG_ERROR("Test Helper: Failed to allocate memory for ProcessedDataChunk.");
        free(buffer);
        return NULL;
    }
    chunk->data = buffer;
    chunk->length = len;
    chunk->data_type = type;
    if (source_id_str) {
        chunk->source_id = strdup(source_id_str);
        if (!chunk->source_id) {
            LOG_WARN("Test Helper: Failed to allocate memory for source_id string.");
        }
    } else {
        chunk->source_id = NULL;
    }
    LOG_INFO("Test Helper: Successfully created chunk from string, length %zu", chunk->length);
    return chunk;
}

// Helper function to load content from a text file into a string
char* load_text_from_file(const char* filepath) {
    LOG_DEBUG("Attempting to open file at path: '%s'", filepath);
    FILE *file = fopen(filepath, "r");
    if (!file) {
        LOG_ERROR("Test Helper: Failed to open file '%s': %s", filepath, strerror(errno));
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = (char*)malloc(length + 1);
    if (!buffer) {
        LOG_ERROR("Test Helper: Memory allocation failed for file content for '%s': %s", filepath, strerror(errno));
        fclose(file);
        return NULL;
    }
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
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

void test_sqlite_text_cleaning() {
    LOG_INFO("--- Running SQLite Text Cleaning Test ---");
    spindex_plugin_init();

    const char* sample_filepath = "test_data/sample_sqlite_preprocessor_test.txt";
    char* original_text = load_text_from_file(sample_filepath);
    assert(original_text != NULL && "Failed to load sample preprocessor test file");

    ProcessedDataChunk *input_chunk = create_chunk_from_string(original_text, CHUNK_TYPE_TEXT_PARSED, "sample_sqlite_preprocessor_test.txt");
    free(original_text);
    assert(input_chunk != NULL && "Failed to create input chunk for cleaning test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);
    assert(output_chunk != NULL && "Preprocessor returned NULL");
    assert(output_chunk->data != NULL && "Preprocessor output data is NULL");
    assert(output_chunk->data_type == CHUNK_TYPE_TEXT_PARSED && "Preprocessor output data_type is not CHUNK_TYPE_TEXT_PARSED");

    const char* expected_cleaned_text = "-- Start of dump\nCREATE TABLE T1 (ID INT PRIMARY KEY NOT NULL, NAME TEXT NOT NULL);\nINSERT INTO \"T1\" VALUES(1,' Hello World ');\nINSERT INTO \"T1\" VALUES(2,'Test Data');\nCREATE TABLE T2 (INFO TEXT);\nINSERT INTO \"T2\" VALUES('Another table content');\n-- End of dump";

    LOG_INFO("Preprocessor Output (length %zu): [%s]", output_chunk->length, (char*)output_chunk->data);
    LOG_INFO("Expected Output (length %zu):   [%s]", strlen(expected_cleaned_text), expected_cleaned_text);

    assert(strcmp((char*)output_chunk->data, expected_cleaned_text) == 0 && "Cleaned text does not match expected output");
    assert(output_chunk->length == strlen(expected_cleaned_text) && "Cleaned text length does not match expected length");

    LOG_INFO("SQLite text cleaning test successful.");
    free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    spindex_plugin_shutdown();
}

void test_null_input_sqlite_preprocessor() {
    LOG_INFO("--- Running NULL Input Test (SQLite Preprocessor) ---");
    spindex_plugin_init();
    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(NULL);
    assert(output_chunk == NULL && "Preprocessor did not return NULL for NULL input chunk");
    LOG_INFO("NULL input chunk test successful (SQLite Preprocessor).");

    ProcessedDataChunk chunk_no_data;
    memset(&chunk_no_data, 0, sizeof(ProcessedDataChunk));
    chunk_no_data.source_id = strdup("test_null_data_sqlite.txt");
    chunk_no_data.data = NULL;
    chunk_no_data.length = 0;
    chunk_no_data.data_type = CHUNK_TYPE_TEXT_PARSED;

    output_chunk = spindex_plugin_process_data(&chunk_no_data);
    assert(output_chunk == NULL && "Preprocessor did not return NULL for chunk with NULL data");
    LOG_INFO("Chunk with NULL data test successful (SQLite Preprocessor).");
    if(chunk_no_data.source_id) free(chunk_no_data.source_id);

    ProcessedDataChunk chunk_wrong_type;
    memset(&chunk_wrong_type, 0, sizeof(ProcessedDataChunk));
    unsigned char dummy_data[] = "test";
    chunk_wrong_type.data = dummy_data;
    chunk_wrong_type.length = strlen((char*)dummy_data);
    chunk_wrong_type.data_type = CHUNK_TYPE_RAW_BYTES;
    chunk_wrong_type.source_id = strdup("test_wrong_type_sqlite.txt");

    output_chunk = spindex_plugin_process_data(&chunk_wrong_type);
    assert(output_chunk == NULL && "Preprocessor did not return NULL for chunk with wrong data type");
    LOG_INFO("Chunk with wrong data type test successful (SQLite Preprocessor).");
    if(chunk_wrong_type.source_id) free(chunk_wrong_type.source_id);

    spindex_plugin_shutdown();
}

int main() {
    LOG_INFO("Starting SQLite Preprocessor tests...");
    PluginInfo info;
    spindex_plugin_get_info(&info);
    LOG_INFO("Plugin Name: %s", info.name);
    LOG_INFO("Plugin Version: %s", info.version);
    LOG_INFO("Plugin Type: %d", info.type);
    LOG_INFO("Plugin Description: %s", info.description);
    assert(strcmp(info.name, "SQLitePreprocessor") == 0);
    assert(strcmp(info.version, "0.2.0") == 0);
    assert(info.type == SPINDEX_PLUGIN_TYPE_PREPROCESSOR);
    assert(strcmp(info.description, "Preprocesses text from SQLite DB dumps.") == 0);

    test_sqlite_text_cleaning();
    test_null_input_sqlite_preprocessor();

    LOG_INFO("All SQLite Preprocessor tests completed.");
    return 0;
}
