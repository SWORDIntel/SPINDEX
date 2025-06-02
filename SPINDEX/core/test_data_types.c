#include "data_types.h" // Assuming compiler will be run from SPINDEX/core or with appropriate include paths
#include <stdio.h>

int main() {
    ProcessedDataChunk chunk;
    chunk.data = NULL;
    chunk.length = 0;
    chunk.source_id = "test_source";

    PluginInfo plugin;
    snprintf(plugin.name, sizeof(plugin.name), "MyTestPlugin");
    snprintf(plugin.version, sizeof(plugin.version), "0.1-alpha");

    printf("Successfully created instances of ProcessedDataChunk and PluginInfo.\n");
    printf("Plugin: %s, Version: %s\n", plugin.name, plugin.version);
    printf("Chunk source: %s\n", chunk.source_id);

    return 0;
}
