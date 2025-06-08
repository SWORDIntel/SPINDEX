#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For tolower

// Assuming standard paths from within SPINDEX/plugins/optimizations/
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"

// Plugin Information
static SpindexPluginInfo generic_text_optimizer_info = {
    "Generic Text Optimizer",
    "0.1.0",
    SPINDEX_PLUGIN_TYPE_OTHER, // Or SPINDEX_PLUGIN_TYPE_ANALYZER
    "Provides generic text optimization/analysis functions (stub)."
};

// Get Plugin Info Function
SPINDEX_PLUGIN_GET_INFO_FUNCTION(spindex_plugin_get_info) {
    LOG_DEBUG("Generic Text Optimizer: spindex_plugin_get_info called.");
    return &generic_text_optimizer_info;
}

// Initialize Function
SPINDEX_PLUGIN_INIT_FUNCTION(spindex_plugin_init) {
    LOG_INFO("Generic Text Optimizer: Initializing plugin (version %s).", generic_text_optimizer_info.version);
    // No specific hardware checks or resources needed for this stub.
    // A real optimization plugin might check for CPU features here if not done by core.
    return 0; // Success
}

// Shutdown Function
SPINDEX_PLUGIN_SHUTDOWN_FUNCTION(spindex_plugin_shutdown) {
    LOG_INFO("Generic Text Optimizer: Shutting down plugin.");
    return 0; // Success
}

// Process Data Function
SPINDEX_PLUGIN_PROCESS_FUNCTION(spindex_plugin_process) {
    LOG_DEBUG("Generic Text Optimizer: Processing data chunk.");

    if (!input_chunk || !input_chunk->data || input_chunk->data_type != CHUNK_TYPE_UTF8_TEXT) {
        LOG_ERROR("Generic Text Optimizer: Received NULL, empty, or non-text input chunk.");
        return NULL;
    }

    // Simple operation: Count occurrences of 'a' (case-insensitive)
    long count = 0;
    const char* text = (const char*)input_chunk->data;
    for (size_t i = 0; i < input_chunk->length; ++i) {
        if (tolower((unsigned char)text[i]) == 'a') {
            count++;
        }
    }

    // Prepare output
    char result_message[256];
    snprintf(result_message, sizeof(result_message),
             "Generic Text Optimizer: Character 'a' (case-insensitive) found %ld times.",
             count);

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("Generic Text Optimizer: Failed to allocate memory for output chunk.");
        return NULL;
    }

    output_chunk->data = (unsigned char*)strdup(result_message);
    if (!output_chunk->data) {
        LOG_ERROR("Generic Text Optimizer: Failed to allocate memory for output data string.");
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = strlen(result_message);
    output_chunk->data_type = CHUNK_TYPE_UTF8_TEXT;

    if (input_chunk->source_id) {
        output_chunk->source_id = strdup(input_chunk->source_id);
        if (!output_chunk->source_id) {
            LOG_WARN("Generic Text Optimizer: Failed to duplicate source_id.");
        }
    } else {
        output_chunk->source_id = NULL;
    }

    LOG_DEBUG("Generic Text Optimizer: Successfully processed data.");
    return output_chunk;
}
