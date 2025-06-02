#include "plugin_interface.h" // This should be found via include path
#include <stdio.h>
#include <stdlib.h> // For NULL
#include <string.h> // For strncpy

// Forward declaration for a dummy get_info function
void dummy_get_plugin_info(PluginInfo *info);
int dummy_init();
void dummy_shutdown();
ProcessedDataChunk* dummy_process(const ProcessedDataChunk *input);

// Dummy implementation of get_plugin_info
void dummy_get_plugin_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, "TestPlugin", sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->version, "1.0", sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
    }
}

// Dummy implementation of init
int dummy_init() {
    printf("Dummy plugin initialized.\n");
    return 0;
}

// Dummy implementation of shutdown
void dummy_shutdown() {
    printf("Dummy plugin shut down.\n");
}

// Dummy implementation of process
ProcessedDataChunk* dummy_process(const ProcessedDataChunk *input) {
    printf("Dummy plugin processing data...\n");
    if (!input) return NULL;

    // For this test, let's just print input info and return a NULL output
    // In a real plugin, new data would be created or input modified.
    printf("Input data length: %zu, source: %s\n", input->length, input->source_id ? input->source_id : "N/A");

    // Example: A real plugin might allocate a new chunk.
    // ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    // if (!output_chunk) return NULL;
    // output_chunk->data = NULL; // Populate as needed
    // output_chunk->length = 0;
    // output_chunk->source_id = "dummy_plugin_output";
    // return output_chunk;

    return NULL; // Or return a pointer to a ProcessedDataChunk
}


int main() {
    SpindexPlugin plugin;

    // Populate PluginInfo using the get_info function
    // In a real scenario, get_info would be called from a dynamically loaded plugin.
    // Here, we call our dummy directly.
    spindex_plugin_get_info_func get_info_ptr = dummy_get_plugin_info;
    get_info_ptr(&plugin.info);

    plugin.init = dummy_init;
    plugin.shutdown = dummy_shutdown;
    plugin.process_data = dummy_process;
    plugin.plugin_handle = NULL; // No actual library loaded for this test

    printf("Plugin Interface Test:\n");
    printf("Plugin Name: %s\n", plugin.info.name);
    printf("Plugin Version: %s\n", plugin.info.version);

    if (plugin.init) {
        plugin.init();
    }

    ProcessedDataChunk test_chunk;
    test_chunk.data = (void*)"Sample data";
    test_chunk.length = strlen("Sample data");
    test_chunk.source_id = "test_main";

    if (plugin.process_data) {
        ProcessedDataChunk *output = plugin.process_data(&test_chunk);
        if (output) {
            printf("Processing returned a chunk (this test dummy returns NULL, so this won't print)\n");
            // Free output chunk if it was allocated by the plugin
            // free(output->data); // If data was allocated
            // free(output);       // If chunk struct itself was allocated
        } else {
            printf("Processing returned NULL as expected by dummy.\n");
        }
    }

    if (plugin.shutdown) {
        plugin.shutdown();
    }

    printf("Plugin interface test complete.\n");

    return 0;
}
