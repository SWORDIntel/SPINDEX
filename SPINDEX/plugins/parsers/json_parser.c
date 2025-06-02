#define _GNU_SOURCE // For strdup if needed
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_parser.h" // Own header (minimal)
#include "../../include/plugin_interface.h" // Defines SpindexPlugin, ProcessedDataChunk, PluginInfo, SpindexPluginType
#include "../../include/utils.h"            // For LOG_INFO, LOG_ERROR etc.
// data_types.h is included via plugin_interface.h

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "JSONParser";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses JSON data. (Currently placeholder implementation)";

// --- Plugin API Functions ---

// Implementation of spindex_plugin_get_info
void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';

        strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';

        info->type = SPINDEX_PLUGIN_TYPE_PARSER;

        strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
}

// Implementation of spindex_plugin_init
// The 'config' and 'plugin_data' parameters are from a more advanced interface idea.
// Current plugin_loader.c doesn't pass these. We'll stick to the current dlsym'd function signatures.
int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No specific data to initialize for this simple version.
    // If we had plugin_data: *plugin_data = NULL;
    LOG_INFO("[%s v%s] Initialization complete.", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0; // Success
}

// Implementation of spindex_plugin_shutdown
// Current plugin_loader.c doesn't pass plugin_data.
void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No specific data to clean up.
    // If we had plugin_data: if (data) free(data);
    LOG_INFO("[%s v%s] Shutdown complete.", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

// Implementation of spindex_plugin_process_data
// Current plugin_loader.c doesn't pass plugin_data.
ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk) {
    LOG_INFO("[%s v%s] Processing data...", G_PLUGIN_NAME, G_PLUGIN_VERSION);

    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("[%s] Input chunk or data is NULL or empty.", G_PLUGIN_NAME);
        return NULL;
    }

    LOG_DEBUG("[%s] Received input data (length %zu, source_id '%s'): %.*s",
              G_PLUGIN_NAME,
              input_chunk->length,
              input_chunk->source_id ? input_chunk->source_id : "N/A",
              100, // Print max 100 chars
              (char*)input_chunk->data);

    // Create a new ProcessedDataChunk for output.
    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate memory for output ProcessedDataChunk.", G_PLUGIN_NAME);
        return NULL;
    }

    // Placeholder processing: Prefix input data with "Parsed: "
    const char *prefix = "Parsed: ";
    size_t prefix_len = strlen(prefix);
    size_t new_data_len = prefix_len + input_chunk->length;

    char *new_data = (char*)malloc(new_data_len + 1); // +1 for null terminator
    if (!new_data) {
        LOG_ERROR("[%s] Failed to allocate memory for new data string.", G_PLUGIN_NAME);
        free(output_chunk);
        return NULL;
    }

    strcpy(new_data, prefix);
    memcpy(new_data + prefix_len, input_chunk->data, input_chunk->length);
    new_data[new_data_len] = '\0';

    output_chunk->data = (void*)new_data;
    output_chunk->length = new_data_len;
    output_chunk->data_type = CHUNK_TYPE_PARSED_JSON_PLACEHOLDER;

    output_chunk->source_id = NULL;
    if (input_chunk->source_id) {
        char source_buf[512];
        snprintf(source_buf, sizeof(source_buf), "%s_parsed_by_%s", input_chunk->source_id, G_PLUGIN_NAME);
        output_chunk->source_id = strdup(source_buf);
        if (!output_chunk->source_id) {
            LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
        }
    }

    LOG_INFO("[%s] Data processing complete. Output length: %zu", G_PLUGIN_NAME, output_chunk->length);
    LOG_DEBUG("[%s] Output data (first 100 bytes): %.*s", G_PLUGIN_NAME, 100, (char*)output_chunk->data);

    return output_chunk;
}
