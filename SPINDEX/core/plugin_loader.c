#include "plugin_loader.h" // Corresponding header, will be found via include path
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> // For directory scanning
#include <dlfcn.h>  // For dynamic loading (dlopen, dlsym, dlclose)

// Stub implementation for load_plugins
int load_plugins(const char *plugin_dir_path, SpindexPlugin **loaded_plugins_array, int *plugin_count) {
    if (!plugin_dir_path || !loaded_plugins_array || !plugin_count) {
        fprintf(stderr, "load_plugins: Invalid arguments provided.\n");
        return -1;
    }

    *loaded_plugins_array = NULL;
    *plugin_count = 0;

    DIR *dir = opendir(plugin_dir_path);
    if (!dir) {
        perror("load_plugins: Failed to open plugin directory");
        return -1; // Or handle more gracefully, maybe no plugins is not critical
    }

    fprintf(stdout, "Scanning for plugins in: %s\n", plugin_dir_path);

    // Allocate space for plugin structures
    // For now, using a fixed max, could be dynamic later
    SpindexPlugin *plugins = (SpindexPlugin*)calloc(MAX_PLUGINS, sizeof(SpindexPlugin));
    if (!plugins) {
        fprintf(stderr, "load_plugins: Failed to allocate memory for plugin array.\n");
        closedir(dir);
        return -1;
    }

    struct dirent *entry;
    int count = 0;
    char full_path[1024];

    while ((entry = readdir(dir)) != NULL && count < MAX_PLUGINS) {
        // Check if the file is a shared object (ends with .so)
        if (strstr(entry->d_name, ".so") == (entry->d_name + strlen(entry->d_name) - 3)) {
            snprintf(full_path, sizeof(full_path), "%s/%s", plugin_dir_path, entry->d_name);
            fprintf(stdout, "Found potential plugin: %s\n", full_path);

            void *handle = dlopen(full_path, RTLD_LAZY);
            if (!handle) {
                fprintf(stderr, "load_plugins: Failed to load plugin %s: %s\n", full_path, dlerror());
                continue; // Skip to next file
            }

            // Load symbols: get_info, init, process_data, shutdown
            spindex_plugin_get_info_func get_info = (spindex_plugin_get_info_func)dlsym(handle, "spindex_plugin_get_info");
            spindex_plugin_init_func init_func = (spindex_plugin_init_func)dlsym(handle, "spindex_plugin_init");
            spindex_plugin_process_func process_func = (spindex_plugin_process_func)dlsym(handle, "spindex_plugin_process_data");
            spindex_plugin_shutdown_func shutdown_func = (spindex_plugin_shutdown_func)dlsym(handle, "spindex_plugin_shutdown");

            if (!get_info || !init_func || !process_func || !shutdown_func) {
                fprintf(stderr, "load_plugins: Plugin %s is missing one or more required symbols: %s\n", full_path, dlerror());
                dlclose(handle);
                continue; // Skip to next file
            }

            // Populate plugin info
            get_info(&plugins[count].info);
            plugins[count].init = init_func;
            plugins[count].process_data = process_func;
            plugins[count].shutdown = shutdown_func;
            plugins[count].plugin_handle = handle;

            // Attempt to initialize the plugin
            if (plugins[count].init() != 0) {
                fprintf(stderr, "load_plugins: Failed to initialize plugin %s\n", plugins[count].info.name);
                dlclose(handle);
                // Reset this slot, or shrink array later
                memset(&plugins[count], 0, sizeof(SpindexPlugin));
                continue;
            }

            fprintf(stdout, "Successfully loaded and initialized plugin: %s (Version: %s)\n", plugins[count].info.name, plugins[count].info.version);
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        fprintf(stdout, "No plugins were loaded.\n");
        free(plugins); // No plugins loaded, free the array
        *loaded_plugins_array = NULL;
    } else {
       *loaded_plugins_array = plugins; // Return the array of loaded plugins
    }

    *plugin_count = count;
    return 0; // Success
}

// Stub implementation for unload_plugins
void unload_plugins(SpindexPlugin *loaded_plugins_array, int plugin_count) {
    if (!loaded_plugins_array || plugin_count == 0) {
        fprintf(stdout, "unload_plugins: No plugins to unload.\n");
        return;
    }

    fprintf(stdout, "Unloading %d plugin(s)...\n", plugin_count);
    for (int i = 0; i < plugin_count; i++) {
        if (loaded_plugins_array[i].plugin_handle) {
            if (loaded_plugins_array[i].shutdown) {
                fprintf(stdout, "Shutting down plugin: %s\n", loaded_plugins_array[i].info.name);
                loaded_plugins_array[i].shutdown();
            }
            dlclose(loaded_plugins_array[i].plugin_handle);
            fprintf(stdout, "Unloaded plugin: %s\n", loaded_plugins_array[i].info.name);
        }
    }

    free(loaded_plugins_array); // Free the array itself
    fprintf(stdout, "All plugins unloaded and resources freed.\n");
}
