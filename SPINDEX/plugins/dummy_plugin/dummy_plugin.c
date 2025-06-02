#include "plugin_interface.h" // Needs to be found via include paths
#include <stdio.h>
#include <string.h> // For strncpy
#include <stdlib.h> // For malloc/free if needed for ProcessedDataChunk

// Plugin metadata
static const char* PLUGIN_NAME = "DummyPlugin";
static const char* PLUGIN_VERSION = "0.1.0";

// Implementation of spindex_plugin_get_info
void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->version, PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
    }
}

// Implementation of spindex_plugin_init
int spindex_plugin_init() {
    printf("[%s] Initializing...\n", PLUGIN_NAME);
    // Perform any one-time setup for the plugin
    printf("[%s] Initialization complete.\n", PLUGIN_NAME);
    return 0; // Success
}

// Implementation of spindex_plugin_shutdown
void spindex_plugin_shutdown() {
    printf("[%s] Shutting down...\n", PLUGIN_NAME);
    // Perform any cleanup
    printf("[%s] Shutdown complete.\n", PLUGIN_NAME);
}

// Implementation of spindex_plugin_process_data
ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input) {
    printf("[%s] Processing data...\n", PLUGIN_NAME);
    if (!input) {
        fprintf(stderr, "[%s] Input chunk is NULL.\n", PLUGIN_NAME);
        return NULL;
    }

    printf("[%s] Received data: Length %zu, Source ID: %s\n",
           PLUGIN_NAME, input->length, input->source_id ? input->source_id : "N/A");

    // Example: Create a new output chunk (or modify input if appropriate)
    // For this dummy, let's create a new chunk and copy the source_id.
    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        perror("Failed to allocate output chunk");
        return NULL;
    }

    // Let's say this plugin converts data to uppercase (if it were char data)
    // For this example, we'll just create a new small data block.
    const char *response_data = "Processed by DummyPlugin";
    output_chunk->data = malloc(strlen(response_data) + 1);
    if (!output_chunk->data) {
        free(output_chunk);
        perror("Failed to allocate data for output_chunk");
        return NULL;
    }
    strcpy((char*)output_chunk->data, response_data);
    output_chunk->length = strlen(response_data);

    // Set a source_id for the output
    size_t source_id_len = strlen(PLUGIN_NAME) + 12; // PLUGIN_NAME + "_output" + null
    output_chunk->source_id = (char*)malloc(source_id_len);
    if (output_chunk->source_id) {
        snprintf(output_chunk->source_id, source_id_len, "%s_output", PLUGIN_NAME);
    } else {
        // Handle allocation failure for source_id, maybe proceed without it or log
        fprintf(stderr, "[%s] Warning: Could not allocate memory for output source_id.\n", PLUGIN_NAME);
    }


    printf("[%s] Produced output: Length %zu, Source ID: %s\n",
        PLUGIN_NAME, output_chunk->length, output_chunk->source_id ? output_chunk->source_id : "N/A");

    return output_chunk;
}
