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
typedef enum {
    CHUNK_TYPE_UNKNOWN = 0,
    CHUNK_TYPE_RAW_BYTES,
    CHUNK_TYPE_UTF8_TEXT,
    CHUNK_TYPE_PREPROCESSED_TEXT, // e.g. after whitespace/comment stripping
    CHUNK_TYPE_PARSED_JSON_PLACEHOLDER, // Placeholder for parsed structure
    CHUNK_TYPE_STRUCTURED_DATA, // Representing actual parsed data in future
    CHUNK_TYPE_TEXT_PARSED,       // For TXTParser output
    CHUNK_TYPE_CSV_PREPROCESSED,  // For CSV preprocessor
    CHUNK_TYPE_CSV_PARSED         // For CSV parser
} ChunkDataType;

typedef struct {
    void *data;         // Pointer to the processed data
    size_t length;      // Length of the data in bytes
    char *source_id;    // Optional: Identifier for the source of this data
    ChunkDataType data_type; // Type of data in the chunk
} ProcessedDataChunk;

// Enum defining different types of plugins
typedef enum {
    SPINDEX_PLUGIN_TYPE_UNKNOWN = 0,
    SPINDEX_PLUGIN_TYPE_PREPROCESSOR,
    SPINDEX_PLUGIN_TYPE_PARSER,
    SPINDEX_PLUGIN_TYPE_INDEXER,
    SPINDEX_PLUGIN_TYPE_DATA_SOURCE,
    SPINDEX_PLUGIN_TYPE_OTHER
} SpindexPluginType;

/**
 * @brief Holds information about a plugin.
 *
 * This structure can be used by the core system to manage plugins
 * and by plugins to identify themselves.
 */
typedef struct {
    char name[256];         // Plugin name
    char version[50];       // Plugin version (e.g., "1.0.2")
    SpindexPluginType type; // Type of the plugin
    char description[512];  // Short description of the plugin's function
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
