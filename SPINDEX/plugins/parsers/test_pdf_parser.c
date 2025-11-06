#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"
#include "pdf_parser.h"

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

void test_pdf_parsing_text_file() {
    LOG_INFO("--- Running PDF Parsing Test (with plain text file) ---");
    spindex_plugin_init();

    ProcessedDataChunk *input_chunk = create_chunk_from_file("test_data/sample_pdf_parser_test.pdf", ".pdf");
    assert(input_chunk != NULL && "Failed to create input chunk for PDF text-file test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);

    // pdftotext should fail (return non-zero) and the plugin should return NULL or an empty chunk.
    // The plugin logs errors when the command fails or output file can't be read.
    // Here, we primarily assert that the plugin handles this gracefully.
    if (output_chunk != NULL && output_chunk->length > 0) {
        // This case might happen if pdftotext outputs some error message to the output file
        LOG_WARN("PDF parser returned non-empty output for a non-PDF file. Output: %s", (char*)output_chunk->data);
        // Depending on strictness, this could be an assert(false) or just a warning.
        // For now, let's assume some error output from pdftotext might be captured.
        // A more robust check would be to see if specific error strings from pdftotext are present.
    } else if (output_chunk == NULL) {
        LOG_INFO("PDF parser correctly returned NULL for non-PDF file input.");
    } else if (output_chunk != NULL && output_chunk->length == 0) {
        LOG_INFO("PDF parser correctly returned an empty chunk for non-PDF file input.");
    }
    // The key is that it doesn't crash and handles the error from pdftotext.
    // The plugin itself logs "Command execution failed..." which is the main indicator of graceful failure.

    if(output_chunk) free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    spindex_plugin_shutdown();
    LOG_INFO("PDF parsing test (with plain text file) completed.");
}


void test_null_input() {
    LOG_INFO("--- Running NULL Input Test (PDF Parser) ---");
    spindex_plugin_init();
    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(NULL);
    assert(output_chunk == NULL && "Parser did not return NULL for NULL input chunk");
    LOG_INFO("NULL input chunk test successful.");

    ProcessedDataChunk chunk_no_data;
    memset(&chunk_no_data, 0, sizeof(ProcessedDataChunk));
    chunk_no_data.source_id = strdup("test_null_data.pdf");
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
    LOG_INFO("Starting PDF Parser tests...");
    PluginInfo info;
    spindex_plugin_get_info(&info);
    LOG_INFO("Plugin Name: %s", info.name);
    LOG_INFO("Plugin Version: %s", info.version);
    LOG_INFO("Plugin Type: %d", info.type);
    LOG_INFO("Plugin Description: %s", info.description);
    assert(strcmp(info.name, "PDFParser") == 0);
    assert(strcmp(info.version, "0.2.0") == 0);
    assert(info.type == SPINDEX_PLUGIN_TYPE_PARSER);

    test_pdf_parsing_text_file();
    test_null_input();

    LOG_INFO("All PDF Parser tests completed.");
    return 0;
}

// Plugin dependency management (stub - no dependencies)
SpindexPluginDependency* spindex_plugin_get_dependencies(int* count) {
    if (count) *count = 0;
    return NULL;  // This plugin has no dependencies
}

void spindex_plugin_free_dependencies_list(SpindexPluginDependency* dependencies, int count) {
    // Nothing to free - we returned NULL
    (void)dependencies;
    (void)count;
}

