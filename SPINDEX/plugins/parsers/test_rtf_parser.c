#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"
#include "rtf_parser.h"

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

void test_rtf_parsing_text_file() {
    LOG_INFO("--- Running RTF Parsing Test (with text file) ---");
    spindex_plugin_init();

    ProcessedDataChunk *input_chunk = create_chunk_from_file("test_data/sample_rtf_parser_test.rtf", ".rtf");
    assert(input_chunk != NULL && "Failed to create input chunk for RTF text-file test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);

    // unrtf might output the text content or parts of it, or fail.
    // If unrtf is not installed, system() will return non-zero, and plugin should return NULL.
    // If unrtf is installed, it might try to interpret the text.
    // We'll check if an error was logged (e.g. command not found, or unrtf internal error).
    if (output_chunk != NULL) {
        LOG_INFO("RTF Parser Output: %s", (char*)output_chunk->data);
        // It's hard to assert specific content here as unrtf might partially parse the text-based "RTF".
        // We're mostly interested if the plugin handles the call and potential errors.
        // A simple check for some of the input text could be done if unrtf passes it through.
        assert(strstr((char*)output_chunk->data, "Hello, this is a basic RTF-like line.") != NULL || strstr((char*)output_chunk->data, "Another line outside RTF structure.") != NULL);
        free_chunk_data(output_chunk);
    } else {
        LOG_WARN("RTF parser returned NULL (e.g. unrtf not found or failed to parse). This is acceptable if unrtf is not installed or the file is truly invalid.");
    }

    free_chunk_data(input_chunk);
    spindex_plugin_shutdown();
    LOG_INFO("RTF parsing test (with text file) completed.");
}


void test_null_input() {
    LOG_INFO("--- Running NULL Input Test (RTF Parser) ---");
    spindex_plugin_init();
    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(NULL);
    assert(output_chunk == NULL && "Parser did not return NULL for NULL input chunk");
    LOG_INFO("NULL input chunk test successful.");

    ProcessedDataChunk chunk_no_data;
    memset(&chunk_no_data, 0, sizeof(ProcessedDataChunk));
    chunk_no_data.source_id = strdup("test_null_data.rtf");
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
    LOG_INFO("Starting RTF Parser tests...");
    PluginInfo info;
    spindex_plugin_get_info(&info);
    LOG_INFO("Plugin Name: %s", info.name);
    LOG_INFO("Plugin Version: %s", info.version);
    LOG_INFO("Plugin Type: %d", info.type);
    LOG_INFO("Plugin Description: %s", info.description);
    assert(strcmp(info.name, "RTFParser") == 0);
    assert(strcmp(info.version, "0.2.0") == 0);
    assert(info.type == SPINDEX_PLUGIN_TYPE_PARSER);

    test_rtf_parsing_text_file();
    test_null_input();

    LOG_INFO("All RTF Parser tests completed.");
    return 0;
}
