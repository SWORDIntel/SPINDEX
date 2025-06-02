#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <stddef.h> // For size_t

/**
 * @brief Represents a chunk of data that has been processed.
 *
 * This structure can be used to pass data between different
 * parts of the system, for example, from a core processing unit
 * to a plugin.
 */
typedef struct {
    void *data;         // Pointer to the processed data
    size_t length;      // Length of the data in bytes
    char *source_id;    // Optional: Identifier for the source of this data
} ProcessedDataChunk;

/**
 * @brief Holds information about a plugin.
 *
 * This structure can be used by the core system to manage plugins
 * and by plugins to identify themselves.
 */
typedef struct {
    char name[256];         // Plugin name
    char version[50];       // Plugin version (e.g., "1.0.2")
    // Potentially add function pointers here later for plugin callbacks
    // void (*init_plugin)();
    // ProcessedDataChunk* (*process_data)(const ProcessedDataChunk* input);
    // void (*shutdown_plugin)();
} PluginInfo;

// Example of a more complex data structure that might be needed later
/**
 * @brief Represents a configuration setting.
 *
 * Could be used to store key-value pairs for application configuration.
 */
typedef struct {
    char key[256];
    char value[1024];
} ConfigSetting;


#endif // DATA_TYPES_H
