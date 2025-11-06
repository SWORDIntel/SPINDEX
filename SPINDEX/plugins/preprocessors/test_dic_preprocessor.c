#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"
#include "dic_preprocessor.h"

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

void test_dicom_text_cleaning() {
    LOG_INFO("--- Running DICOM Text Cleaning Test ---");
    spindex_plugin_init();

    const char* sample_filepath = "test_data/sample_dic_preprocessor_test.txt";
    char* original_text = load_text_from_file(sample_filepath);
    assert(original_text != NULL && "Failed to load sample preprocessor test file");

    ProcessedDataChunk *input_chunk = create_chunk_from_string(original_text, CHUNK_TYPE_TEXT_PARSED, "sample_dic_preprocessor_test.txt");
    free(original_text);
    assert(input_chunk != NULL && "Failed to create input chunk for cleaning test");

    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(input_chunk);
    assert(output_chunk != NULL && "Preprocessor returned NULL");
    assert(output_chunk->data != NULL && "Preprocessor output data is NULL");
    assert(output_chunk->data_type == CHUNK_TYPE_TEXT_PARSED && "Preprocessor output data_type is not CHUNK_TYPE_TEXT_PARSED");

    // Input: "  dicom output with   irregular  spacing\n  another medical line\t\tEND  "
    // Expected: "dicom output with irregular spacing\nanother medical line END"
    const char* expected_cleaned_text = "dicom output with irregular spacing\nanother medical line END";

    LOG_INFO("Preprocessor Output: [%s]", (char*)output_chunk->data);
    LOG_INFO("Expected Output:   [%s]", expected_cleaned_text);

    assert(strcmp((char*)output_chunk->data, expected_cleaned_text) == 0 && "Cleaned text does not match expected output");
    assert(output_chunk->length == strlen(expected_cleaned_text) && "Cleaned text length does not match expected length");

    LOG_INFO("DICOM text cleaning test successful.");
    free_chunk_data(output_chunk);
    free_chunk_data(input_chunk);
    spindex_plugin_shutdown();
}

void test_null_input_dicom_preprocessor() {
    LOG_INFO("--- Running NULL Input Test (DICOM Preprocessor) ---");
    spindex_plugin_init();
    ProcessedDataChunk *output_chunk = spindex_plugin_process_data(NULL);
    assert(output_chunk == NULL && "Preprocessor did not return NULL for NULL input chunk");
    LOG_INFO("NULL input chunk test successful (DICOM Preprocessor).");

    ProcessedDataChunk chunk_no_data;
    memset(&chunk_no_data, 0, sizeof(ProcessedDataChunk));
    chunk_no_data.source_id = strdup("test_null_data_dicom.txt");
    chunk_no_data.data = NULL;
    chunk_no_data.length = 0;
    chunk_no_data.data_type = CHUNK_TYPE_TEXT_PARSED;

    output_chunk = spindex_plugin_process_data(&chunk_no_data);
    assert(output_chunk == NULL && "Preprocessor did not return NULL for chunk with NULL data");
    LOG_INFO("Chunk with NULL data test successful (DICOM Preprocessor).");
    if(chunk_no_data.source_id) free(chunk_no_data.source_id);

    ProcessedDataChunk chunk_wrong_type;
    memset(&chunk_wrong_type, 0, sizeof(ProcessedDataChunk));
    unsigned char dummy_data[] = "test";
    chunk_wrong_type.data = dummy_data;
    chunk_wrong_type.length = strlen((char*)dummy_data);
    chunk_wrong_type.data_type = CHUNK_TYPE_RAW_BYTES;
    chunk_wrong_type.source_id = strdup("test_wrong_type_dicom.txt");

    output_chunk = spindex_plugin_process_data(&chunk_wrong_type);
    assert(output_chunk == NULL && "Preprocessor did not return NULL for chunk with wrong data type");
    LOG_INFO("Chunk with wrong data type test successful (DICOM Preprocessor).");
    if(chunk_wrong_type.source_id) free(chunk_wrong_type.source_id);

    spindex_plugin_shutdown();
}

int main() {
    LOG_INFO("Starting DICOM Preprocessor tests...");
    PluginInfo info;
    spindex_plugin_get_info(&info);
    LOG_INFO("Plugin Name: %s", info.name);
    LOG_INFO("Plugin Version: %s", info.version);
    LOG_INFO("Plugin Type: %d", info.type);
    LOG_INFO("Plugin Description: %s", info.description);
    assert(strcmp(info.name, "DICOMPreprocessor") == 0);
    assert(strcmp(info.version, "0.2.0") == 0);
    assert(info.type == SPINDEX_PLUGIN_TYPE_PREPROCESSOR);
    assert(strcmp(info.description, "Preprocesses text extracted from DICOM files.") == 0);

    test_dicom_text_cleaning();
    test_null_input_dicom_preprocessor();

    LOG_INFO("All DICOM Preprocessor tests completed.");
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

