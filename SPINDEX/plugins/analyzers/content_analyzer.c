#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strstr, strchr, memcpy, strdup

// Adjust paths as necessary
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"          // For LOG_INFO, LOG_DEBUG, LOG_ERROR

// --- Forward declarations for plugin functions ---
void analyzer_get_info(PluginInfo *info);
int analyzer_init();
void analyzer_shutdown();
ProcessedDataChunk* analyzer_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
void spindex_plugin_get_info(PluginInfo *info) {
    analyzer_get_info(info);
}

int spindex_plugin_init() {
    return analyzer_init();
}

void spindex_plugin_shutdown() {
    analyzer_shutdown();
}

ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return analyzer_process_data(input_chunk);
}

// --- Plugin Implementation ---

void analyzer_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "Heuristic Content Analyzer", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, "0.1", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    // This plugin analyzes content, which can be seen as a preprocessing step
    // that enriches data or helps guide subsequent processing stages.
    info->type = SPINDEX_PLUGIN_TYPE_PREPROCESSOR; 

    strncpy(info->description, "Analyzes content using heuristic rules (e.g., keyword spotting).", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int analyzer_init() {
    LOG_INFO("Heuristic Content Analyzer plugin initializing...");
    // No specific initialization required for these simple heuristics.
    return 0; // Success
}

void analyzer_shutdown() {
    LOG_INFO("Heuristic Content Analyzer plugin shutting down...");
    // No specific resources to release.
}

ProcessedDataChunk* analyzer_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk) {
        LOG_ERROR("Heuristic Analyzer: Received NULL input chunk.");
        return NULL;
    }
     if (!input_chunk->data && input_chunk->length > 0) {
        LOG_ERROR("Heuristic Analyzer: Input chunk has non-zero length but NULL data.");
        return NULL;
    }

    LOG_DEBUG("Heuristic Analyzer: process_data called for chunk from source '%s' with length %zu.",
              input_chunk->source_id ? input_chunk->source_id : "N/A",
              input_chunk->length);

    // Implement Simple Heuristics
    char* text_content = NULL;
    if (input_chunk->data && input_chunk->length > 0) {
        // Create a null-terminated copy for safe string operations.
        // This is important as input_chunk->data is not guaranteed to be null-terminated.
        text_content = (char*)malloc(input_chunk->length + 1);
        if (text_content) {
            memcpy(text_content, input_chunk->data, input_chunk->length);
            text_content[input_chunk->length] = '\0';

            // Heuristic 1: Check for "SQL" or "SELECT" (case-sensitive)
            // For more robust detection, case-insensitive search (e.g., strcasestr or manual conversion) would be better.
            if (strstr(text_content, "SQL") != NULL || strstr(text_content, "SELECT") != NULL || strstr(text_content, "INSERT") != NULL || strstr(text_content, "UPDATE") != NULL || strstr(text_content, "DELETE") != NULL || strstr(text_content, "CREATE TABLE") != NULL) {
                LOG_INFO("Heuristic Analyzer: Detected potential SQL content in source '%s'.", input_chunk->source_id ? input_chunk->source_id : "N/A");
            }

            // Heuristic 2: Check for "<?xml"
            if (strstr(text_content, "<?xml") != NULL) {
                LOG_INFO("Heuristic Analyzer: Detected potential XML content in source '%s'.", input_chunk->source_id ? input_chunk->source_id : "N/A");
            }
            
            // Heuristic 3: Check for "{" and "}" as possible JSON (very basic)
            // This is a weak heuristic and might catch many non-JSON text files.
            // A more robust check might look for balanced braces, quotes around keys, etc.
            if (strchr(text_content, '{') != NULL && strchr(text_content, '}') != NULL) {
                 LOG_INFO("Heuristic Analyzer: Detected potential JSON-like structures in source '%s'.", input_chunk->source_id ? input_chunk->source_id : "N/A");
            }

            // Heuristic 4: Check for common script tags or shebangs
            if (strstr(text_content, "<script") != NULL || strstr(text_content, "#!/bin/bash") != NULL || strstr(text_content, "#!/usr/bin/python") != NULL ) {
                 LOG_INFO("Heuristic Analyzer: Detected potential script content (HTML script or shell/python script) in source '%s'.", input_chunk->source_id ? input_chunk->source_id : "N/A");
            }

            free(text_content);
        } else {
            LOG_ERROR("Heuristic Analyzer: Failed to allocate memory for text analysis buffer.");
            // Continue to return a copy of the input chunk, as analysis failure shouldn't stop the pipeline.
        }
    } else {
        LOG_DEBUG("Heuristic Analyzer: Input chunk data is NULL or length is 0, skipping heuristic analysis.");
    }

    // Return a new copy of the input chunk.
    // This plugin, in its current form, primarily logs findings rather than modifying the chunk.
    // Returning a copy is consistent with the plugin contract where output is a new chunk.
    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("Heuristic Analyzer: Failed to allocate memory for output chunk.");
        return NULL;
    }

    output_chunk->length = input_chunk->length;
    if (input_chunk->data && input_chunk->length > 0) {
        output_chunk->data = malloc(input_chunk->length +1); // +1 for safety if used as string later, though not strictly necessary for raw copy
        if (!output_chunk->data) {
            free(output_chunk);
            LOG_ERROR("Heuristic Analyzer: Failed to allocate memory for output chunk data.");
            return NULL;
        }
        memcpy(output_chunk->data, input_chunk->data, input_chunk->length);
        ((char*)output_chunk->data)[input_chunk->length] = '\0'; // Null-terminate for safety
    } else {
        output_chunk->data = NULL; // Handle empty input chunk data
    }

    output_chunk->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
    if (input_chunk->source_id && !output_chunk->source_id) {
        LOG_WARN("Heuristic Analyzer: Failed to duplicate source_id for output_chunk.");
    }
    output_chunk->data_type = input_chunk->data_type; // Preserve original data type

    LOG_DEBUG("Heuristic Analyzer: Returning a copy of the input chunk.");
    return output_chunk; // Caller is responsible for freeing this.
}
