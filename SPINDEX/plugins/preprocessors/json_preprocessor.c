#define _GNU_SOURCE // For strdup if used by underlying functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace

#include "plugin_interface.h" // Defines SpindexPlugin, ProcessedDataChunk, PluginInfo
#include "utils.h"            // For LOG_INFO, LOG_ERROR etc.
// #include "data_types.h" // data_types.h is included by plugin_interface.h

// --- Forward declarations for functions that were part of the original file ---
// Simplified versions or stubs will be used for now.
static char *plugin_trim_whitespace(const char *str);
// static char *plugin_detect_and_convert_to_utf8(const char *data, size_t data_length); // Stubbed
// static char *plugin_normalize_unicode(const char *utf8_string); // Stubbed
static char *plugin_internal_preprocess_json(const char *raw_content);


// --- Plugin Implementation ---

static const char* G_PLUGIN_NAME = "JSONPreprocessor";
static const char* G_PLUGIN_VERSION = "0.2.0"; // Updated version after refactor

// Implementation of spindex_plugin_get_info
void spindex_plugin_get_info(PluginInfo *info) { // Renamed
    if (info) {
        strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
        info->type = SPINDEX_PLUGIN_TYPE_PREPROCESSOR; // Set the type
        strncpy(info->description, "Basic JSON Preprocessor (trims, checks structure)", sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
}

// Implementation of spindex_plugin_init
int spindex_plugin_init() { // Renamed
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // Any one-time setup for the plugin
    LOG_INFO("[%s v%s] Initialization complete.", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0; // Success
}

// Implementation of spindex_plugin_shutdown
void spindex_plugin_shutdown() { // Renamed
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // Perform any cleanup
    LOG_INFO("[%s v%s] Shutdown complete.", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

// Implementation of spindex_plugin_process_data
ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input) { // Renamed
    LOG_INFO("[%s v%s] Processing data...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    if (!input || !input->data || input->length == 0) {
        LOG_ERROR("[%s] Input chunk, data, or length is invalid/empty.", G_PLUGIN_NAME);
        return NULL;
    }

    LOG_DEBUG("[%s] Received input data (first 50 bytes): %.*s", G_PLUGIN_NAME, 50, (char*)input->data);

    // The input data is a char* string from the file.
    char *raw_json_string = (char*)input->data;

    // Call the core preprocessing logic (simplified for now)
    char *processed_json_string = plugin_internal_preprocess_json(raw_json_string);

    if (!processed_json_string) {
        LOG_ERROR("[%s] Preprocessing failed.", G_PLUGIN_NAME);
        return NULL; // Or return a chunk with an error flag / empty data
    }

    // Package the processed string into a new ProcessedDataChunk
    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate memory for output ProcessedDataChunk.", G_PLUGIN_NAME);
        free(processed_json_string); // Clean up the processed string
        return NULL;
    }

    output_chunk->data = (void*)processed_json_string; // The string itself
    output_chunk->length = strlen(processed_json_string);
    output_chunk->data_type = CHUNK_TYPE_PREPROCESSED_TEXT; // Set appropriate type
    output_chunk->source_id = NULL; // Preprocessor might not change source_id, or could append to it
    if (input->source_id) { // Try to retain or modify source_id
        char source_buf[512];
        snprintf(source_buf, sizeof(source_buf), "%s_preprocessed_by_%s", input->source_id, G_PLUGIN_NAME);
        output_chunk->source_id = strdup(source_buf);
        if (!output_chunk->source_id) {
            LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
        }
    }

    LOG_INFO("[%s] Preprocessing complete. Output length: %zu", G_PLUGIN_NAME, output_chunk->length);
    LOG_DEBUG("[%s] Processed data (first 50 bytes): %.*s", G_PLUGIN_NAME, 50, (char*)output_chunk->data);

    return output_chunk;
}


// --- Internal Helper Functions (adapted from original file, simplified) ---

static char *plugin_trim_whitespace(const char *str) {
    if (!str) {
        return NULL;
    }
    const char *start_ptr = str;
    const char *end_ptr;
    size_t len;

    while (isspace((unsigned char)*start_ptr)) {
        start_ptr++;
    }

    if (*start_ptr == 0) {
        return strdup(""); // All spaces or empty
    }

    end_ptr = start_ptr + strlen(start_ptr) - 1;
    while (end_ptr > start_ptr && isspace((unsigned char)*end_ptr)) {
        end_ptr--;
    }
    len = end_ptr - start_ptr + 1;

    char *trimmed_str = (char *)malloc(len + 1);
    if (!trimmed_str) {
        LOG_ERROR("[%s] trim_whitespace: malloc failed.", G_PLUGIN_NAME);
        return NULL;
    }
    strncpy(trimmed_str, start_ptr, len);
    trimmed_str[len] = '\0';
    return trimmed_str;
}

/* Stubbed/Simplified versions of complex processing from original file */
/*
static char *plugin_detect_and_convert_to_utf8(const char *data, size_t data_length) {
    LOG_WARN("[%s] Encoding detection and UTF-8 conversion is currently stubbed. Assuming UTF-8.", G_PLUGIN_NAME);
    // In a real version, this would use uchardet and iconv
    char *out_data = (char *)malloc(data_length + 1);
    if (!out_data) return NULL;
    memcpy(out_data, data, data_length);
    out_data[data_length] = '\0';
    return out_data;
}

static char *plugin_normalize_unicode(const char *utf8_string) {
    LOG_WARN("[%s] Unicode normalization is currently stubbed.", G_PLUGIN_NAME);
    // In a real version, this would use unorm
    return strdup(utf8_string);
}
*/

static char *plugin_internal_preprocess_json(const char *raw_content) {
    if (!raw_content) {
        LOG_ERROR("[%s] plugin_internal_preprocess_json: Raw content is NULL.", G_PLUGIN_NAME);
        return NULL;
    }
    LOG_DEBUG("[%s] plugin_internal_preprocess_json: Starting with raw content.", G_PLUGIN_NAME);

    // Step 1: Encoding detection and conversion (stubbed for now, assume valid char*)
    // char *utf8_content = plugin_detect_and_convert_to_utf8(raw_content, strlen(raw_content));
    // if (!utf8_content) {
    //     LOG_ERROR("[%s] Failed to convert content to UTF-8 (stubbed).", G_PLUGIN_NAME);
    //     return NULL;
    // }
    // For now, just use raw_content as if it's already UTF-8
    char *current_content = strdup(raw_content); // Work on a copy
    if (!current_content) {
        LOG_ERROR("[%s] strdup failed for raw_content.", G_PLUGIN_NAME);
        return NULL;
    }

    // Step 2: Unicode normalization (stubbed for now)
    // char *normalized_content = plugin_normalize_unicode(current_content);
    // free(current_content); // Free previous step's result
    // if (!normalized_content) {
    //    LOG_ERROR("[%s] Failed to normalize Unicode content (stubbed).", G_PLUGIN_NAME);
    //    return NULL;
    // }
    // current_content = normalized_content;

    // Step 3: Trim whitespace (using the local helper)
    char *trimmed_content = plugin_trim_whitespace(current_content);
    free(current_content); // Free previous step's result
    if (!trimmed_content) {
        LOG_ERROR("[%s] Memory allocation failed while trimming content.", G_PLUGIN_NAME);
        return NULL;
    }
    current_content = trimmed_content;

    // Step 4: Basic structural check/wrap (from original file)
    // Check if the content starts with valid JSON characters
    if (current_content[0] != '{' && current_content[0] != '[') {
        LOG_DEBUG("[%s] Content does not start with '{' or '['. Wrapping.", G_PLUGIN_NAME);
        size_t len = strlen(current_content) + 10; // Add space for wrapping "[{...}]"
        char *processed_content_wrapped = (char *)malloc(len);
        if (!processed_content_wrapped) {
            LOG_ERROR("[%s] Memory allocation failed while attempting to wrap JSON content.", G_PLUGIN_NAME);
            free(current_content);
            return NULL;
        }
        // This wrapping logic might be too simple or aggressive.
        // A real JSON preprocessor might just strip comments or leading/trailing non-JSON chars.
        // For now, keeping the original logic if it's not a clear object/array start.
        snprintf(processed_content_wrapped, len, "[{\"data\":\"%s\"}]", current_content); // Simplified wrapping
        LOG_INFO("[%s] Content wrapped for JSON compatibility.", G_PLUGIN_NAME);
        free(current_content);
        current_content = processed_content_wrapped;
    } else {
        LOG_DEBUG("[%s] Content appears to start with valid JSON char.", G_PLUGIN_NAME);
    }

    LOG_DEBUG("[%s] plugin_internal_preprocess_json: Finished.", G_PLUGIN_NAME);
    return current_content; // This string must be freed by the caller (or by ProcessedDataChunk freeing)
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
