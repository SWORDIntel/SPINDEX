#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "plugin_interface.h" // Defines SpindexPlugin and related types

#define MAX_PLUGINS 10 // Maximum number of plugins that can be loaded

/**
 * @brief Scans a directory for plugin shared objects (.so files), loads them,
 *        and initializes them.
 *
 * The function looks for .so files, attempts to open them, retrieves plugin
 * metadata and function pointers (init, process_data, shutdown).
 *
 * @param plugin_dir_path Path to the directory containing plugin .so files.
 * @param loaded_plugins_array A pointer to an array of SpindexPlugin structs.
 *                             This array will be allocated by this function and
 *                             filled with successfully loaded plugins. The caller
 *                             is responsible for freeing this array using unload_plugins
 *                             or directly if this function fails partway.
 * @param plugin_count A pointer to an integer that will be filled with the
 *                     number of plugins successfully loaded.
 * @return 0 on success, -1 on critical failure (e.g., cannot allocate memory).
 *         Individual plugin load failures will be reported to stderr but won't
 *         necessarily cause a -1 return unless no plugins can be loaded or a
 *         critical error occurs.
 */
int load_plugins(const char *plugin_dir_path, SpindexPlugin **loaded_plugins_array, int *plugin_count);

/**
 * @brief Unloads all previously loaded plugins.
 *
 * Calls the shutdown function for each plugin and then unloads the shared library.
 * Frees the array of SpindexPlugin structs allocated by load_plugins.
 *
 * @param loaded_plugins_array The array of loaded plugins.
 * @param plugin_count The number of plugins in the array.
 */
void unload_plugins(SpindexPlugin *loaded_plugins_array, int plugin_count);

#endif // PLUGIN_LOADER_H
