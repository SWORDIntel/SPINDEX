#include "plugin_loader.h"
#include "plugin_interface.h" // For SpindexPluginDependency etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdbool.h> // For bool type

// --- SemVer Parsing and Comparison Utility ---
typedef struct {
    int major;
    int minor;
    int patch;
    // Pre-release and build metadata ignored for simplicity in this version
} SemVer;

// Parses "X.Y.Z" or "vX.Y.Z" into SemVer struct. Returns true on success.
// Very basic, assumes valid input format without pre-release/build metadata.
static bool parse_semver(const char *version_str, SemVer *version) {
    if (!version_str || !version) return false;
    const char *ptr = version_str;
    if (*ptr == 'v') ptr++; // Skip 'v' prefix if present
    if (sscanf(ptr, "%d.%d.%d", &version->major, &version->minor, &version->patch) == 3) {
        return true;
    }
    return false;
}

// Compares two SemVer structs.
// Returns: 0 if v1 == v2, >0 if v1 > v2, <0 if v1 < v2.
static int compare_semver(const SemVer *v1, const SemVer *v2) {
    if (v1->major != v2->major) return v1->major - v2->major;
    if (v1->minor != v2->minor) return v1->minor - v2->minor;
    if (v1->patch != v2->patch) return v1->patch - v2->patch;
    return 0;
}

// Checks if 'actual_version_str' satisfies 'spec_str'.
// Supports: "X.Y.Z" (exact) and ">=X.Y.Z".
static bool check_version_spec(const char *actual_version_str, const char *spec_str) {
    SemVer actual_ver, spec_ver;
    if (!parse_semver(actual_version_str, &actual_ver)) {
        fprintf(stderr, "Failed to parse actual version: %s\n", actual_version_str);
        return false; // Cannot compare if actual version is invalid
    }

    if (strncmp(spec_str, ">=", 2) == 0) {
        if (!parse_semver(spec_str + 2, &spec_ver)) {
            fprintf(stderr, "Failed to parse spec version (>=): %s\n", spec_str + 2);
            return false;
        }
        return compare_semver(&actual_ver, &spec_ver) >= 0;
    } else { // Exact match
        if (!parse_semver(spec_str, &spec_ver)) {
            fprintf(stderr, "Failed to parse spec version (exact): %s\n", spec_str);
            return false;
        }
        return compare_semver(&actual_ver, &spec_ver) == 0;
    }
}

// --- Internal structure for discovered plugin data ---
typedef struct {
    char path[1024];
    void *handle;
    PluginInfo info; // Includes name and version string
    SpindexPluginDependency *dependencies;
    int dependency_count;
    spindex_plugin_free_dependencies_list_func free_dependencies_list;

    spindex_plugin_init_func init_func;
    spindex_plugin_process_func process_func;
    spindex_plugin_shutdown_func shutdown_func;

    bool metadata_loaded; // True if info and dependencies were successfully loaded
    bool marked_for_load; // True if dependencies and versions are met
    bool initialized;     // True if init_func succeeded
    int load_order_index; // For topological sort result, -1 if not determined
} DiscoveredPlugin;


// --- Plugin Loading Logic ---

// Helper to find a discovered plugin by name
static DiscoveredPlugin* find_discovered_plugin(const char* name, DiscoveredPlugin* all_plugins, int total_discovered) {
    for (int i = 0; i < total_discovered; ++i) {
        if (all_plugins[i].metadata_loaded && strcmp(all_plugins[i].info.name, name) == 0) {
            return &all_plugins[i];
        }
    }
    return NULL;
}


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
        return 0; // No plugins is not necessarily a critical failure for the loader itself
    }
    fprintf(stdout, "PluginLoader: Scanning for plugins in: %s\n", plugin_dir_path);

    DiscoveredPlugin *discovered_plugins = NULL;
    int discovered_capacity = 0;
    int discovered_count = 0;

    struct dirent *entry;
    char full_path[1024];

    // --- Pass 1: Discover plugins and load metadata/dependencies ---
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".so") != (entry->d_name + strlen(entry->d_name) - 3)) {
            continue; // Not a .so file
        }

        if (discovered_count >= discovered_capacity) {
            int new_capacity = (discovered_capacity == 0) ? INITIAL_PLUGIN_CAPACITY : discovered_capacity * 2;
            DiscoveredPlugin *new_array = (DiscoveredPlugin*)realloc(discovered_plugins, new_capacity * sizeof(DiscoveredPlugin));
            if (!new_array) {
                fprintf(stderr, "PluginLoader: Failed to reallocate memory for discovered plugins array.\n");
                // Cleanup already opened handles and dependency lists
                for (int i = 0; i < discovered_count; ++i) {
                    if (discovered_plugins[i].dependencies && discovered_plugins[i].free_dependencies_list) {
                        discovered_plugins[i].free_dependencies_list(discovered_plugins[i].dependencies, discovered_plugins[i].dependency_count);
                    }
                    if (discovered_plugins[i].handle) dlclose(discovered_plugins[i].handle);
                }
                free(discovered_plugins);
                closedir(dir);
                return -1; // Critical memory allocation failure
            }
            discovered_plugins = new_array;
            discovered_capacity = new_capacity;
        }

        memset(&discovered_plugins[discovered_count], 0, sizeof(DiscoveredPlugin)); // Initialize struct
        snprintf(discovered_plugins[discovered_count].path, sizeof(discovered_plugins[discovered_count].path), "%s/%s", plugin_dir_path, entry->d_name);
        fprintf(stdout, "PluginLoader: Found potential plugin: %s\n", discovered_plugins[discovered_count].path);

        void *handle = dlopen(discovered_plugins[discovered_count].path, RTLD_LAZY | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "PluginLoader: Failed to load plugin %s: %s\n", discovered_plugins[discovered_count].path, dlerror());
            continue; // Skip this file, don't increment discovered_count for this one
        }
        discovered_plugins[discovered_count].handle = handle;

        spindex_plugin_get_info_func get_info = (spindex_plugin_get_info_func)dlsym(handle, "spindex_plugin_get_info");
        spindex_plugin_get_dependencies_func get_deps = (spindex_plugin_get_dependencies_func)dlsym(handle, "spindex_plugin_get_dependencies");
        spindex_plugin_free_dependencies_list_func free_deps = (spindex_plugin_free_dependencies_list_func)dlsym(handle, "spindex_plugin_free_dependencies_list");

        if (!get_info || !get_deps || !free_deps) {
            fprintf(stderr, "PluginLoader: Plugin %s is missing one or more required metadata functions (get_info, get_dependencies, free_dependencies_list): %s\n", discovered_plugins[discovered_count].path, dlerror());
            dlclose(handle);
            discovered_plugins[discovered_count].handle = NULL; // Mark as not usable
            continue;
        }

        get_info(&discovered_plugins[discovered_count].info);
        discovered_plugins[discovered_count].dependencies = get_deps(&discovered_plugins[discovered_count].dependency_count);
        discovered_plugins[discovered_count].free_dependencies_list = free_deps;
        discovered_plugins[discovered_count].metadata_loaded = true;

        // dlsym for main functions now, but don't call init yet
        discovered_plugins[discovered_count].init_func = (spindex_plugin_init_func)dlsym(handle, "spindex_plugin_init");
        discovered_plugins[discovered_count].process_func = (spindex_plugin_process_func)dlsym(handle, "spindex_plugin_process_data");
        discovered_plugins[discovered_count].shutdown_func = (spindex_plugin_shutdown_func)dlsym(handle, "spindex_plugin_shutdown");

        if (!discovered_plugins[discovered_count].init_func || !discovered_plugins[discovered_count].process_func || !discovered_plugins[discovered_count].shutdown_func) {
             fprintf(stderr, "PluginLoader: Plugin %s (%s) is missing one or more core lifecycle symbols (init, process_data, shutdown): %s\n",
                discovered_plugins[discovered_count].info.name, discovered_plugins[discovered_count].path, dlerror());
            if (discovered_plugins[discovered_count].dependencies && discovered_plugins[discovered_count].free_dependencies_list) {
                discovered_plugins[discovered_count].free_dependencies_list(discovered_plugins[discovered_count].dependencies, discovered_plugins[discovered_count].dependency_count);
            }
            dlclose(handle);
            discovered_plugins[discovered_count].handle = NULL;
            discovered_plugins[discovered_count].metadata_loaded = false; // Mark as not fully loaded
            continue;
        }

        fprintf(stdout, "PluginLoader: Discovered plugin '%s' version '%s' from %s. Dependencies: %d\n",
                discovered_plugins[discovered_count].info.name,
                discovered_plugins[discovered_count].info.version,
                discovered_plugins[discovered_count].path,
                discovered_plugins[discovered_count].dependency_count);

        discovered_count++;
    }
    closedir(dir);

    if (discovered_count == 0) {
        fprintf(stdout, "PluginLoader: No valid plugins found in directory.\n");
        free(discovered_plugins);
        return 0;
    }

    // --- Pass 2: Dependency Resolution & Initialization ---
    // Simple iterative resolution for now. A full graph + topological sort is more robust for complex cases.
    // This approach tries to initialize plugins whose dependencies are already met.
    int final_loaded_count = 0;
    SpindexPlugin *final_plugins = (SpindexPlugin*)calloc(discovered_count, sizeof(SpindexPlugin));
    if (!final_plugins) {
        fprintf(stderr, "PluginLoader: Failed to allocate memory for final plugin array.\n");
        // Cleanup discovered_plugins
        for (int i = 0; i < discovered_count; ++i) {
            if (discovered_plugins[i].dependencies && discovered_plugins[i].free_dependencies_list) {
                discovered_plugins[i].free_dependencies_list(discovered_plugins[i].dependencies, discovered_plugins[i].dependency_count);
            }
            if (discovered_plugins[i].handle) dlclose(discovered_plugins[i].handle);
        }
        free(discovered_plugins);
        return -1;
    }

    int initialized_this_round;
    int max_rounds = discovered_count; // Avoid infinite loop in case of unresolvable circular deps (basic check)

    for (int round = 0; round < max_rounds; ++round) {
        initialized_this_round = 0;
        for (int i = 0; i < discovered_count; ++i) {
            if (!discovered_plugins[i].metadata_loaded || discovered_plugins[i].initialized || !discovered_plugins[i].handle) {
                continue; // Already initialized, failed metadata load, or handle closed
            }

            bool deps_met = true;
            for (int j = 0; j < discovered_plugins[i].dependency_count; ++j) {
                SpindexPluginDependency *dep = &discovered_plugins[i].dependencies[j];
                DiscoveredPlugin *dep_plugin_ptr = find_discovered_plugin(dep->name, discovered_plugins, discovered_count);

                if (!dep_plugin_ptr || !dep_plugin_ptr->initialized) { // Dependency not found or not yet initialized
                    deps_met = false;
                    break;
                }
                // Version check
                if (!check_version_spec(dep_plugin_ptr->info.version, dep->required_version_spec)) {
                    fprintf(stderr, "PluginLoader: Plugin '%s' dependency '%s' version mismatch. Required: '%s', Found: '%s'.\n",
                            discovered_plugins[i].info.name, dep->name, dep->required_version_spec, dep_plugin_ptr->info.version);
                    deps_met = false;
                    break;
                }
            }

            if (deps_met) {
                fprintf(stdout, "PluginLoader: Initializing plugin '%s'...\n", discovered_plugins[i].info.name);
                if (discovered_plugins[i].init_func() == 0) {
                    final_plugins[final_loaded_count].info = discovered_plugins[i].info;
                    final_plugins[final_loaded_count].init = discovered_plugins[i].init_func;
                    final_plugins[final_loaded_count].process_data = discovered_plugins[i].process_func;
                    final_plugins[final_loaded_count].shutdown = discovered_plugins[i].shutdown_func;
                    final_plugins[final_loaded_count].plugin_handle = discovered_plugins[i].handle;

                    discovered_plugins[i].initialized = true;
                    final_loaded_count++;
                    initialized_this_round++;
                    fprintf(stdout, "PluginLoader: Successfully initialized plugin: %s (Version: %s)\n",
                            final_plugins[final_loaded_count-1].info.name, final_plugins[final_loaded_count-1].info.version);
                } else {
                    fprintf(stderr, "PluginLoader: Failed to initialize plugin %s\n", discovered_plugins[i].info.name);
                    dlclose(discovered_plugins[i].handle); // Close handle on init failure
                    discovered_plugins[i].handle = NULL;
                    discovered_plugins[i].metadata_loaded = false; // Mark as failed
                }
            }
        }
        if (initialized_this_round == 0 && final_loaded_count < discovered_count) {
            // No plugins were initialized in this round, but some are still pending.
            // This implies circular dependencies or unmet dependencies for remaining plugins.
            for(int i=0; i < discovered_count; ++i) {
                if(discovered_plugins[i].metadata_loaded && !discovered_plugins[i].initialized && discovered_plugins[i].handle) {
                     fprintf(stderr, "PluginLoader: Failed to load plugin '%s' due to unmet dependencies or circular dependency.\n", discovered_plugins[i].info.name);
                     // Free its specific resources now as it won't be loaded
                     if (discovered_plugins[i].dependencies && discovered_plugins[i].free_dependencies_list) {
                        discovered_plugins[i].free_dependencies_list(discovered_plugins[i].dependencies, discovered_plugins[i].dependency_count);
                        discovered_plugins[i].dependencies = NULL;
                     }
                     dlclose(discovered_plugins[i].handle);
                     discovered_plugins[i].handle = NULL;
                }
            }
            break; // Exit loop
        }
        if (final_loaded_count == discovered_count) break; // All successfully discovered plugins processed
    }

    // Free dependency lists for successfully loaded plugins (loader now owns this info implicitly via SpindexPlugin.info)
    // and for plugins that were discovered but not loaded.
    for (int i = 0; i < discovered_count; ++i) {
        if (discovered_plugins[i].dependencies && discovered_plugins[i].free_dependencies_list) {
            discovered_plugins[i].free_dependencies_list(discovered_plugins[i].dependencies, discovered_plugins[i].dependency_count);
        }
        // Handles for plugins that were NOT successfully initialized and added to final_plugins should be closed.
        // This is done above if init fails or if marked unloadable due to deps.
        // If a plugin was discovered, metadata loaded, but then couldn't be init due to deps in the loop, its handle also gets closed.
    }
    free(discovered_plugins);


    if (final_loaded_count == 0) {
        fprintf(stdout, "PluginLoader: No plugins were successfully loaded and initialized.\n");
        free(final_plugins);
        *loaded_plugins_array = NULL;
    } else {
        // Shrink final_plugins array if needed (though calloc was for discovered_count, which might be larger than final_loaded_count)
        SpindexPlugin *shrunk_plugins = (SpindexPlugin*)realloc(final_plugins, final_loaded_count * sizeof(SpindexPlugin));
        if(shrunk_plugins == NULL && final_loaded_count > 0) { // realloc failed but we had plugins
            fprintf(stderr, "PluginLoader: Warning - realloc failed to shrink plugin array, using original.\n");
            *loaded_plugins_array = final_plugins; // Use original potentially oversized array
        } else {
            *loaded_plugins_array = shrunk_plugins; // Use shrunk array (or NULL if final_loaded_count was 0)
        }
    }
    *plugin_count = final_loaded_count;
    return 0; // Success
}


void unload_plugins(SpindexPlugin *loaded_plugins_array, int plugin_count) {
    if (!loaded_plugins_array || plugin_count == 0) {
        // fprintf(stdout, "PluginLoader: No plugins to unload.\n"); // Can be too verbose
        return;
    }

    fprintf(stdout, "PluginLoader: Unloading %d plugin(s)...\n", plugin_count);
    for (int i = 0; i < plugin_count; i++) {
        if (loaded_plugins_array[i].plugin_handle) { // Check handle as it's the key for loaded lib
            if (loaded_plugins_array[i].shutdown) {
                fprintf(stdout, "PluginLoader: Shutting down plugin: %s\n", loaded_plugins_array[i].info.name);
                loaded_plugins_array[i].shutdown();
            }
            dlclose(loaded_plugins_array[i].plugin_handle);
            fprintf(stdout, "PluginLoader: Unloaded plugin library for: %s\n", loaded_plugins_array[i].info.name);
        }
    }

    free(loaded_plugins_array);
    fprintf(stdout, "PluginLoader: All plugins unloaded and plugin array freed.\n");
}

// Definition for INITIAL_PLUGIN_CAPACITY if not in header
#ifndef INITIAL_PLUGIN_CAPACITY
#define INITIAL_PLUGIN_CAPACITY 5 // Initial capacity for dynamic array
#endif
