#define _GNU_SOURCE // For strdup
#include "txt_preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace

#include "plugin_interface.h" // For ProcessedDataChunk, PluginInfo
#include "data_types.h"       // For SpindexPluginType, ChunkDataType
#include "utils.h"            // For logging

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "TXTPreprocessor";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Preprocesses plain text files (e.g., trims whitespace).";

// Helper for whitespace trimming
static char* trim_whitespace(const char* str) {
    if (!str) return NULL;
    const char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == 0) return strdup(""); // All whitespace or empty

    const char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    size_t len = (end - start) + 1;
    char* trimmed = (char*)malloc(len + 1);
    if (!trimmed) {
        LOG_ERROR("[%s] trim_whitespace: Failed to allocate memory for trimmed string", G_PLUGIN_NAME);
        return NULL;
    }
    strncpy(trimmed, start, len);
    trimmed[len] = '\0';
    return trimmed;
}

// --- Plugin API Functions ---
void spindex_plugin_get_info(PluginInfo* info) {
    if (!info) return;
    strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';

    info->type = SPINDEX_PLUGIN_TYPE_PREPROCESSOR;

    strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int spindex_plugin_init() { // Matches current plugin_loader
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No specific plugin_data for this plugin yet
    return 0;
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk* input_chunk) { // Matches current plugin_loader
    if (!input_chunk || !input_chunk->data) {
        LOG_ERROR("[%s] Received null input chunk or data.", G_PLUGIN_NAME);
        return NULL;
    }
    LOG_DEBUG("[%s] Processing data for source_id: %s", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "N/A");

    char* original_data_str = (char*)input_chunk->data;
    char* trimmed_data = trim_whitespace(original_data_str);

    if (!trimmed_data) {
        LOG_ERROR("[%s] Failed to trim whitespace. Duplicating original as fallback.", G_PLUGIN_NAME);
        trimmed_data = strdup(original_data_str); // Fallback to duplicating original data
        if (!trimmed_data) {
             LOG_ERROR("[%s] Failed to duplicate original data on trim failure.", G_PLUGIN_NAME);
             return NULL; // Cannot proceed
        }
    }

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate output chunk.", G_PLUGIN_NAME);
        free(trimmed_data); // Free the (potentially) newly allocated string
        return NULL;
    }

    output_chunk->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
    if (input_chunk->source_id && !output_chunk->source_id) {
        LOG_WARN("[%s] Failed to duplicate source_id.", G_PLUGIN_NAME);
    }

    output_chunk->data = (void*)trimmed_data;
    output_chunk->length = strlen(trimmed_data);
    output_chunk->data_type = CHUNK_TYPE_PREPROCESSED_TEXT;

    LOG_DEBUG("[%s] Finished processing source_id: %s. Output length: %zu",
              G_PLUGIN_NAME, output_chunk->source_id ? output_chunk->source_id : "N/A", output_chunk->length);
    return output_chunk;
}

void spindex_plugin_shutdown() { // Matches current plugin_loader
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No specific data to clean up
}
