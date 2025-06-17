#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h> // For size_t

// Maximum length for string-based configuration values
#define MAX_CONFIG_STR_LEN 256

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} LogLevel;

/**
 * @brief Holds the overall application configuration.
 *
 * This structure is populated by the config_manager by parsing a
 * configuration file.
 */
typedef struct {
    char plugin_directory[MAX_CONFIG_STR_LEN];
    char log_file_path[MAX_CONFIG_STR_LEN];
    LogLevel log_level;
    int max_concurrent_files;
    size_t input_buffer_size;

    // Elasticsearch settings
    char *elasticsearch_url;        // Dynamically allocated
    char *elasticsearch_index_name; // Dynamically allocated

    // Active plugins
    char *active_indexer_name;      // Dynamically allocated, e.g., "ElasticsearchIndexer"

    // NLP NER specific settings
    int nlp_ner_enabled;
    char nlp_ner_python_executable[MAX_CONFIG_STR_LEN];
    char nlp_ner_script_path[MAX_CONFIG_STR_LEN];

    // Add other configuration parameters as needed
    // For example:
    // char output_directory[MAX_CONFIG_STR_LEN];
    // int network_timeout_ms;

} SpindexConfig;

#endif // CONFIG_H
