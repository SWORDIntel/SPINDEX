#define _GNU_SOURCE // For strdup, strcasecmp
#include "config_manager.h" // Corresponding header
#include "utils.h"          // For LOG_WARN etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace, tolower
#include <strings.h> // For strcasecmp

// Helper function to trim leading/trailing whitespace from a string
static char* trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = 0;

    return str;
}

// Helper function to convert string to LogLevel
static LogLevel string_to_loglevel(const char *str) {
    if (strcasecmp(str, "ERROR") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "WARN") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(str, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "TRACE") == 0) return LOG_LEVEL_TRACE;
    return LOG_LEVEL_INFO; // Default
}

static const char* loglevel_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

void set_default_config(SpindexConfig *config) {
    if (!config) return;

    strncpy(config->plugin_directory, "plugins/", MAX_CONFIG_STR_LEN -1);
    config->plugin_directory[MAX_CONFIG_STR_LEN -1] = '\0';

    strncpy(config->log_file_path, "spindex.log", MAX_CONFIG_STR_LEN -1);
    config->log_file_path[MAX_CONFIG_STR_LEN -1] = '\0';

    config->log_level = LOG_LEVEL_INFO;
    config->max_concurrent_files = 10;
    config->input_buffer_size = 4096; // 4KB

    // Free existing if any, then strdup default
    if (config->elasticsearch_url) free(config->elasticsearch_url);
    config->elasticsearch_url = strdup("http://localhost:9200");

    if (config->elasticsearch_index_name) free(config->elasticsearch_index_name);
    config->elasticsearch_index_name = strdup("spindex_default_idx");

    if (config->active_indexer_name) free(config->active_indexer_name);
    config->active_indexer_name = strdup("ElasticsearchIndexer"); // Default active indexer

    // Default NLP NER settings
    config->nlp_ner_enabled = 0; // Disabled by default
    strncpy(config->nlp_ner_python_executable, "python3", MAX_CONFIG_STR_LEN - 1);
    config->nlp_ner_python_executable[MAX_CONFIG_STR_LEN - 1] = '\0';
    strncpy(config->nlp_ner_script_path, "plugins/aegis_modules/scripts/ner_spacy.py", MAX_CONFIG_STR_LEN - 1); // Adjusted default path
    config->nlp_ner_script_path[MAX_CONFIG_STR_LEN - 1] = '\0';

    if (!config->elasticsearch_url || !config->elasticsearch_index_name || !config->active_indexer_name) {
        // Handle strdup failure, though simple for this context
        fprintf(stderr, "set_default_config: strdup failed for ES or Indexer settings.\n");
    }
}

int parse_config_file(const char *filepath, SpindexConfig *config) {
    if (!filepath || !config) return -1;

    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("parse_config_file: Error opening file");
        return -1; // Cannot open file
    }

    char line[512]; // Max line length for config
    int line_number = 0;

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        char *trimmed_line = trim_whitespace(line);

        if (*trimmed_line == '#' || *trimmed_line == ';' || *trimmed_line == '\0') {
            continue; // Skip comments and empty lines
        }

        char *key = trimmed_line;
        char *value = strchr(trimmed_line, '=');

        if (!value) {
            fprintf(stderr, "parse_config_file: Malformed line %d (no '=' found): %s\n", line_number, trimmed_line);
            // Continue parsing other lines, or return -2 for stricter parsing
            continue;
        }

        *value = '\0'; // Null-terminate the key
        value++;       // Move to the start of the value

        key = trim_whitespace(key);
        value = trim_whitespace(value);

        // Remove inline comments from the value
        char *comment_char = strpbrk(value, "#;");
        if (comment_char != NULL) {
            *comment_char = '\0'; // Terminate value string before comment
            value = trim_whitespace(value); // Trim again if space before comment
        }

        if (strcmp(key, "plugin_directory") == 0) {
            strncpy(config->plugin_directory, value, MAX_CONFIG_STR_LEN - 1);
            config->plugin_directory[MAX_CONFIG_STR_LEN - 1] = '\0';
        } else if (strcmp(key, "log_file_path") == 0) {
            strncpy(config->log_file_path, value, MAX_CONFIG_STR_LEN - 1);
            config->log_file_path[MAX_CONFIG_STR_LEN - 1] = '\0';
        } else if (strcmp(key, "log_level") == 0) {
            config->log_level = string_to_loglevel(value);
        } else if (strcmp(key, "max_concurrent_files") == 0) {
            config->max_concurrent_files = atoi(value);
        } else if (strcmp(key, "input_buffer_size") == 0) {
            config->input_buffer_size = (size_t)atol(value);
        } else if (strcmp(key, "elasticsearch_url") == 0) {
            if (config->elasticsearch_url) free(config->elasticsearch_url);
            config->elasticsearch_url = strdup(value);
            if (!config->elasticsearch_url) fprintf(stderr, "parse_config_file: strdup failed for elasticsearch_url\n");
        } else if (strcmp(key, "elasticsearch_index_name") == 0) {
            if (config->elasticsearch_index_name) free(config->elasticsearch_index_name);
            config->elasticsearch_index_name = strdup(value);
            if (!config->elasticsearch_index_name) fprintf(stderr, "parse_config_file: strdup failed for elasticsearch_index_name\n");
        } else if (strcmp(key, "active_indexer_name") == 0) {
            if (config->active_indexer_name) free(config->active_indexer_name);
            config->active_indexer_name = strdup(value);
            if (!config->active_indexer_name) fprintf(stderr, "parse_config_file: strdup failed for active_indexer_name\n");
        } else if (strcmp(key, "active_preprocessor_name") == 0) {
            // These fields are not currently in SpindexConfig struct, but parsing them defensively.
            LOG_WARN("Config key 'active_preprocessor_name' found but not used by SpindexConfig directly yet.");
            // if (config->active_preprocessor_name) free(config->active_preprocessor_name);
            // config->active_preprocessor_name = strdup(value);
        } else if (strcmp(key, "active_parser_name") == 0) {
            LOG_WARN("Config key 'active_parser_name' found but not used by SpindexConfig directly yet.");
            // if (config->active_parser_name) free(config->active_parser_name);
            // config->active_parser_name = strdup(value);
        } else if (strcmp(key, "NLP_NER_ENABLED") == 0) {
            config->nlp_ner_enabled = atoi(value);
        } else if (strcmp(key, "NLP_NER_PYTHON_EXECUTABLE") == 0) {
            strncpy(config->nlp_ner_python_executable, value, MAX_CONFIG_STR_LEN - 1);
            config->nlp_ner_python_executable[MAX_CONFIG_STR_LEN - 1] = '\0';
        } else if (strcmp(key, "NLP_NER_SCRIPT_PATH") == 0) {
            strncpy(config->nlp_ner_script_path, value, MAX_CONFIG_STR_LEN - 1);
            config->nlp_ner_script_path[MAX_CONFIG_STR_LEN - 1] = '\0';
        } else {
            fprintf(stderr, "parse_config_file: Unknown configuration key on line %d: %s\n", line_number, key);
        }
    }

    fclose(file);
    return 0; // Success
}

void free_config_resources(SpindexConfig *config) {
    // Currently, SpindexConfig uses fixed-size char arrays and simple types.
    // If any char* members were dynamically allocated (e.g., with strdup),
    // they would need to be freed here.
    if (config) {
        if (config->elasticsearch_url) {
            free(config->elasticsearch_url);
            config->elasticsearch_url = NULL;
        }
        if (config->elasticsearch_index_name) {
            free(config->elasticsearch_index_name);
            config->elasticsearch_index_name = NULL;
        }
        if (config->active_indexer_name) {
            free(config->active_indexer_name);
            config->active_indexer_name = NULL;
        }
    }
}

void print_config(const SpindexConfig *config) {
    if (!config) {
        printf("Config is NULL.\n");
        return;
    }
    printf("--- Spindex Configuration ---\n");
    printf("Plugin Directory:     %s\n", config->plugin_directory);
    printf("Log File Path:        %s\n", config->log_file_path);
    printf("Log Level:            %s (%d)\n", loglevel_to_string(config->log_level), config->log_level);
    printf("Max Concurrent Files: %d\n", config->max_concurrent_files);
    printf("Input Buffer Size:    %zu\n", config->input_buffer_size);
    printf("Elasticsearch URL:    %s\n", config->elasticsearch_url ? config->elasticsearch_url : "N/A");
    printf("Elasticsearch Index:  %s\n", config->elasticsearch_index_name ? config->elasticsearch_index_name : "N/A");
    printf("Active Indexer Name:  %s\n", config->active_indexer_name ? config->active_indexer_name : "N/A");
    printf("NLP NER Enabled:      %d\n", config->nlp_ner_enabled);
    printf("NLP NER Python Exec:  %s\n", config->nlp_ner_python_executable);
    printf("NLP NER Script Path:  %s\n", config->nlp_ner_script_path);
    printf("---------------------------\n");
}
