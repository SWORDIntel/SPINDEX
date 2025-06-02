#include "config_manager.h" // Corresponding header
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
    // For example:
    // if (config && config->some_dynamic_string) {
    //     free(config->some_dynamic_string);
    //     config->some_dynamic_string = NULL;
    // }
    (void)config; // Unused parameter for now
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
    printf("---------------------------\n");
}
