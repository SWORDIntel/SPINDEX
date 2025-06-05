#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Ensure strdup is declared
#include <assert.h>
#include <errno.h> // For strerror(errno)

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h" // For LOG_INFO, LOG_ERROR, etc.
#include "doc_parser.h" // Functions to test: spindex_plugin_get_info, init, shutdown, process_data

// Helper function to create a ProcessedDataChunk from a file's content
ProcessedDataChunk* create_chunk_from_file(const char* filepath, const char* source_id_suffix) {
    LOG_DEBUG("Attempting to open file at path: '%s'", filepath); // Added for debugging
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

void test_doc_parsing() {
    LOG_INFO("--- Running DOC Parsing Test ---");
    spindex_plugin_init();

    ProcessedDataChunk *input_chunk = create_chunk_from_file("test_data/sample_doc_parser_test.doc", ".doc");
    assert(input_chunk != NULL && "Failed to create input chunk for DOC test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);
    assert(output_chunk != NULL && "Parser returned NULL for DOC file");
    assert(output_chunk->data != NULL && "Parser output data is NULL for DOC file");
    assert(output_chunk->length > 0 && "Parser output data length is 0 for DOC file");
    assert(output_chunk->data_type == CHUNK_TYPE_TEXT_PARSED && "Parser output data_type is not CHUNK_TYPE_TEXT_PARSED for DOC");

    LOG_INFO("DOC Parser Output: %s", (char*)output_chunk->data);
    assert(strstr((char*)output_chunk->data, "This is a test DOC file") != NULL && "Key phrase 'This is a test DOC file' not found in DOC output");
    assert(strstr((char*)output_chunk->data, "Hello world") != NULL && "Key phrase 'Hello world' not found in DOC output");

    LOG_INFO("DOC parsing test successful.");
    free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    spindex_plugin_shutdown();
}

void test_docx_parsing() {
    LOG_INFO("--- Running DOCX Parsing Test ---");
    spindex_plugin_init();

    ProcessedDataChunk *input_chunk = create_chunk_from_file("test_data/sample_doc_parser_test.docx", ".docx");
    assert(input_chunk != NULL && "Failed to create input chunk for DOCX test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);
    assert(output_chunk != NULL && "Parser returned NULL for DOCX file");
    assert(output_chunk->data != NULL && "Parser output data is NULL for DOCX file");
    assert(output_chunk->length > 0 && "Parser output data length is 0 for DOCX file");
    assert(output_chunk->data_type == CHUNK_TYPE_TEXT_PARSED && "Parser output data_type is not CHUNK_TYPE_TEXT_PARSED for DOCX");

    LOG_INFO("DOCX Parser Output: %s", (char*)output_chunk->data);
    assert(strstr((char*)output_chunk->data, "This is a test DOCX file") != NULL && "Key phrase 'This is a test DOCX file' not found in DOCX output");
    assert(strstr((char*)output_chunk->data, "Hello again") != NULL && "Key phrase 'Hello again' not found in DOCX output");

    LOG_INFO("DOCX parsing test successful.");
    free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    spindex_plugin_shutdown();
}

void test_null_input() {
    LOG_INFO("--- Running NULL Input Test ---");
    spindex_plugin_init();
    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(NULL);
    assert(output_chunk == NULL && "Parser did not return NULL for NULL input chunk");
    LOG_INFO("NULL input chunk test successful.");

    ProcessedDataChunk chunk_no_data;
    memset(&chunk_no_data, 0, sizeof(ProcessedDataChunk));
    chunk_no_data.source_id = strdup("test_null_data.doc");
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
    LOG_INFO("Starting DOC Parser tests...");
    PluginInfo info;
    spindex_plugin_get_info(&info);
    LOG_INFO("Plugin Name: %s", info.name);
    LOG_INFO("Plugin Version: %s", info.version);
    LOG_INFO("Plugin Type: %d", info.type);
    LOG_INFO("Plugin Description: %s", info.description);
    assert(strcmp(info.name, "DOCParser") == 0);
    assert(strcmp(info.version, "0.2.0") == 0);
    assert(info.type == SPINDEX_PLUGIN_TYPE_PARSER);

    test_doc_parsing();
    test_docx_parsing();
    test_null_input();

    LOG_INFO("All DOC Parser tests completed.");
    return 0;
}
