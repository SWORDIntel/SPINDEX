#define _GNU_SOURCE // For strdup
#include "csv_preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <ctype.h> // Not used in this pass-through version

#include "plugin_interface.h" // For ProcessedDataChunk, PluginInfo
#include "data_types.h"       // For SpindexPluginType, ChunkDataType
#include "utils.h"            // For logging

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "CSVPreprocessor";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Preprocesses CSV files (currently pass-through).";

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

int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No specific plugin_data for this plugin
    return 0;
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk* input_chunk) {
    if (!input_chunk || !input_chunk->data) {
        LOG_ERROR("[%s] Received null input chunk or data.", G_PLUGIN_NAME);
        return NULL;
    }
    LOG_DEBUG("[%s] Processing data for source_id: %s (pass-through)",
              G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "N/A");

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate output chunk.", G_PLUGIN_NAME);
        return NULL;
    }

    output_chunk->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
    if (input_chunk->source_id && !output_chunk->source_id) {
        LOG_WARN("[%s] Failed to strdup source_id.", G_PLUGIN_NAME);
        // Continue even if source_id duplication fails for a pass-through
    }

    output_chunk->data = (void*)strdup((char*)input_chunk->data); // Pass-through, duplicate data
    if (!output_chunk->data) {
        LOG_ERROR("[%s] Failed to strdup data.", G_PLUGIN_NAME);
        if (output_chunk->source_id) free(output_chunk->source_id);
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = input_chunk->length; // Length is the same as input
    output_chunk->data_type = CHUNK_TYPE_CSV_PREPROCESSED;

    LOG_DEBUG("[%s] Finished processing source_id: %s. Output length: %zu",
              G_PLUGIN_NAME, output_chunk->source_id ? output_chunk->source_id : "N/A", output_chunk->length);
    return output_chunk;
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No specific data to clean up
}
