#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include "data_types.h" // For ProcessedDataChunk, PluginInfo

/**
 * @brief Function pointer type for plugin initialization.
 *
 * This function is called when the plugin is loaded.
 * It should perform any setup required by the plugin.
 *
 * @return 0 on success, non-zero on failure.
 */
typedef int (*spindex_plugin_init_func)();

/**
 * @brief Function pointer type for plugin shutdown.
 *
 * This function is called when the plugin is unloaded.
 * It should perform any cleanup required by the plugin,
 * such as freeing allocated resources.
 */
typedef void (*spindex_plugin_shutdown_func)();

/**
 * @brief Function pointer type for data processing by a plugin.
 *
 * This function is called by the core to have the plugin process data.
 * The plugin is responsible for managing the memory of the output chunk
 * if it differs from the input chunk.
 *
 * @param input A pointer to the ProcessedDataChunk to be processed.
 * @return A pointer to a ProcessedDataChunk containing the result.
 *         The plugin may return the input pointer if modified in-place,
 *         or a new chunk. Returns NULL on failure or if no output is generated.
 */
typedef ProcessedDataChunk* (*spindex_plugin_process_func)(const ProcessedDataChunk *input);

/**
 * @brief Function pointer type for getting plugin information.
 *
 * This function is called to retrieve metadata about the plugin.
 * The plugin should populate the provided PluginInfo struct.
 *
 * @param info A pointer to a PluginInfo struct to be filled by the plugin.
 */
typedef void (*spindex_plugin_get_info_func)(PluginInfo *info);


/**
 * @brief Represents a loaded SPINDEX plugin.
 *
 * This structure holds the function pointers that define the plugin's
 * behavior, allowing the core system to interact with it.
 * It also holds the plugin's metadata.
 */
typedef struct {
    PluginInfo info;                            // Information about the plugin
    spindex_plugin_init_func init;              // Initialization function
    spindex_plugin_shutdown_func shutdown;      // Shutdown function
    spindex_plugin_process_func process_data;   // Data processing function
    // spindex_plugin_get_info_func get_info; // get_info is used to populate PluginInfo
                                            // so it's usually called before this struct is fully formed.
                                            // Alternatively, it could be part of a registration process.
    void *plugin_handle; // Handle for the loaded shared library (e.g., from dlopen)
} SpindexPlugin;

#endif // PLUGIN_INTERFACE_H
