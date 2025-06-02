#include "plugin_loader.h" // To test this module
#include "data_types.h"    // For ProcessedDataChunk
#include <stdio.h>
#include <stdlib.h> // For free
#include <string.h> // For strlen

int main() {
    SpindexPlugin *loaded_plugins = NULL;
    int plugin_count = 0;
    const char *plugin_dir = "/app/SPINDEX/plugins/dummy_plugin"; // More specific path for first test
    // const char *plugin_dir = "/app/SPINDEX/plugins"; // Generic path for broader testing

    printf("--- Starting Plugin Loader Test ---\n");

    printf("Attempting to load plugins from: %s\n", plugin_dir);
    if (load_plugins(plugin_dir, &loaded_plugins, &plugin_count) == 0) {
        printf("load_plugins call completed. Found %d plugin(s).\n", plugin_count);
    } else {
        fprintf(stderr, "load_plugins call failed critically. Exiting.\n");
        return 1;
    }

    if (plugin_count > 0 && loaded_plugins) {
        printf("\n--- Iterating Loaded Plugins ---\n");
        for (int i = 0; i < plugin_count; i++) {
            printf("Plugin %d:\n", i + 1);
            printf("  Name: %s\n", loaded_plugins[i].info.name);
            printf("  Version: %s\n", loaded_plugins[i].info.version);
            printf("  Plugin Handle: %p\n", loaded_plugins[i].plugin_handle);

            // Test processing with a dummy chunk
            if (loaded_plugins[i].process_data) {
                printf("  Attempting to process data with plugin %s...\n", loaded_plugins[i].info.name);
                ProcessedDataChunk input_chunk;
                char *test_data_str = "Hello from loader test!";
                input_chunk.data = (void*)test_data_str;
                input_chunk.length = strlen(test_data_str);
                input_chunk.source_id = "test_plugin_loader";

                ProcessedDataChunk *output_chunk = loaded_plugins[i].process_data(&input_chunk);
                if (output_chunk) {
                    printf("  Plugin %s processed data. Output source: %s, Length: %zu\n",
                           loaded_plugins[i].info.name,
                           output_chunk->source_id ? output_chunk->source_id : "N/A",
                           output_chunk->length);
                    if (output_chunk->data) {
                        printf("  Output data content (first 50 chars): %.*s\n", 50, (char*)output_chunk->data);
                    }

                    // Important: Free resources allocated by the plugin
                    if (output_chunk->source_id) free(output_chunk->source_id);
                    if (output_chunk->data) free(output_chunk->data);
                    free(output_chunk);
                } else {
                    printf("  Plugin %s returned NULL from process_data.\n", loaded_plugins[i].info.name);
                }
            }
            printf("-----\n");
        }
    } else {
        printf("No plugins were loaded, or 'loaded_plugins' array is NULL.\n");
    }

    printf("\n--- Unloading Plugins ---\n");
    unload_plugins(loaded_plugins, plugin_count); // unload_plugins handles NULL array or count == 0

    printf("--- Plugin Loader Test Complete ---\n");
    return 0;
}
