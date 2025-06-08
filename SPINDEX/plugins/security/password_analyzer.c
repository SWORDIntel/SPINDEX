#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h" // For LOG_INFO, LOG_ERROR, etc.

#define RESULT_BUFFER_SIZE 8192
#define MAX_LINE_LENGTH 1024 // For context snippet
#define MAX_POTENTIAL_PASSWORD_LENGTH 128 // Max length for an extracted password value
#define MIN_POTENTIAL_PASSWORD_LENGTH 6

// --- Forward declarations for plugin functions ---
void password_analyzer_get_info(PluginInfo *info);
int password_analyzer_init();
void password_analyzer_shutdown();
ProcessedDataChunk* password_analyzer_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
void spindex_plugin_get_info(PluginInfo *info) {
    password_analyzer_get_info(info);
}

int spindex_plugin_init() {
    return password_analyzer_init();
}

void spindex_plugin_shutdown() {
    password_analyzer_shutdown();
}

ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return password_analyzer_process_data(input_chunk);
}

// --- Plugin Implementation ---

void password_analyzer_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "Password Analyzer", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    strncpy(info->version, "0.1", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    info->type = SPINDEX_PLUGIN_TYPE_SECURITY;
    strncpy(info->description, "Identifies potential passwords near keywords in text.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int password_analyzer_init() {
    LOG_INFO("Password Analyzer plugin initialized.");
    return 0;
}

void password_analyzer_shutdown() {
    LOG_INFO("Password Analyzer plugin shutting down.");
}

// Basic check for password-like characteristics (very naive)
static bool is_potential_password_value(const char* value) {
    if (!value) return false;
    size_t len = strlen(value);
    if (len < MIN_POTENTIAL_PASSWORD_LENGTH || len > MAX_POTENTIAL_PASSWORD_LENGTH) return false;

    bool has_alpha = false;
    bool has_digit = false;
    // bool has_special = false; // Could add later

    for (size_t i = 0; i < len; ++i) {
        if (isalpha(value[i])) has_alpha = true;
        else if (isdigit(value[i])) has_digit = true;
        // else if (!isspace(value[i])) has_special = true;
    }
    // For now, just check it's not purely one type or too short/long
    return (has_alpha || has_digit); // At least some alphanumeric content
}

// Helper to escape minimal JSON special characters in a string
// Writes to dest, ensures null termination. Returns number of chars written to dest (excluding null).
// Caller must ensure dest_size is adequate.
static size_t escape_json_string(const char* src, char* dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return 0;
    size_t i = 0, j = 0;
    while (src[i] != '\0' && j < dest_size - 1) {
        if (src[i] == '"') {
            if (j + 1 < dest_size - 1) { // Need space for \ and "
                dest[j++] = '\\';
                dest[j++] = '"';
            } else break; // Not enough space
        } else if (src[i] == '\\') {
            if (j + 1 < dest_size - 1) {
                dest[j++] = '\\';
                dest[j++] = '\\';
            } else break;
        } else {
            dest[j++] = src[i];
        }
        i++;
    }
    dest[j] = '\0';
    return j;
}


ProcessedDataChunk* password_analyzer_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("PasswordAnalyzer: Input chunk or its data is NULL or empty.");
        return NULL;
    }

    char* input_copy = strdup(input_chunk->data);
    if (!input_copy) {
        LOG_ERROR("PasswordAnalyzer: Failed to strdup input data.");
        return NULL;
    }

    char result_buffer[RESULT_BUFFER_SIZE] = {0};
    int current_buffer_pos = 0;
    current_buffer_pos += snprintf(result_buffer, RESULT_BUFFER_SIZE, "{\"potential_passwords\": [");
    bool first_finding = true;

    const char* keywords[] = {"password", "passwd", "pass", "secret", "key", "token", "credential", "apikey", "privatekey"};
    size_t num_keywords = sizeof(keywords) / sizeof(keywords[0]);

    char* line_saveptr;
    char* line = strtok_r(input_copy, "\n", &line_saveptr);
    int line_number = 0;

    while (line != NULL) {
        line_number++;
        for (size_t k_idx = 0; k_idx < num_keywords; ++k_idx) {
            const char* keyword = keywords[k_idx];
            char* found_keyword_ptr = strstr(line, keyword); // Case-sensitive for now

            if (found_keyword_ptr) {
                char potential_value[MAX_POTENTIAL_PASSWORD_LENGTH + 1] = {0};
                char* value_start = found_keyword_ptr + strlen(keyword);

                // Skip common delimiters like ':', '=', ' '
                while (*value_start != '\0' && (*value_start == ':' || *value_start == '=' || isspace(*value_start))) {
                    value_start++;
                }

                // Extract next sequence of non-space characters
                if (*value_start != '\0') { // Check if not end of line after delimiters
                    char* value_end = value_start;
                    while (*value_end != '\0' && !isspace(*value_end) && (value_end - value_start) < MAX_POTENTIAL_PASSWORD_LENGTH) {
                        value_end++;
                    }
                    strncpy(potential_value, value_start, value_end - value_start);
                    potential_value[value_end - value_start] = '\0'; // Null terminate

                    if (is_potential_password_value(potential_value)) {
                        if (!first_finding) {
                            if (current_buffer_pos < RESULT_BUFFER_SIZE - 1) {
                                result_buffer[current_buffer_pos++] = ',';
                            } else { LOG_WARN("PasswordAnalyzer: Buffer full before comma for finding."); goto end_loops; }
                        }
                        first_finding = false;

                        char escaped_value[MAX_POTENTIAL_PASSWORD_LENGTH * 2 + 1]; // For JSON escaping
                        char escaped_context[MAX_LINE_LENGTH * 2 + 1];

                        escape_json_string(potential_value, escaped_value, sizeof(escaped_value));

                        // For context, take a snippet of the line, ensure it's not too long for buffer
                        char context_snippet[MAX_LINE_LENGTH + 1];
                        strncpy(context_snippet, line, MAX_LINE_LENGTH);
                        context_snippet[MAX_LINE_LENGTH] = '\0';
                        escape_json_string(context_snippet, escaped_context, sizeof(escaped_context));

                        int written = snprintf(result_buffer + current_buffer_pos,
                                               RESULT_BUFFER_SIZE - current_buffer_pos,
                                               "{\"keyword\": \"%s\", \"value\": \"%s\", \"context_line\": \"%s\", \"line_number\": %d}",
                                               keyword, escaped_value, escaped_context, line_number);

                        if (written < 0 || current_buffer_pos + written >= RESULT_BUFFER_SIZE -1) {
                            LOG_WARN("PasswordAnalyzer: Buffer truncated while writing finding.");
                            if(current_buffer_pos > 0 && result_buffer[current_buffer_pos-1] == ',') {
                                current_buffer_pos--; result_buffer[current_buffer_pos] = '\0';
                            }
                            goto end_loops;
                        }
                        current_buffer_pos += written;
                        LOG_DEBUG("PasswordAnalyzer: Found potential password near '%s' on line %d", keyword, line_number);
                    }
                }
            }
        }
        line = strtok_r(NULL, "\n", &line_saveptr);
    }

end_loops:
    free(input_copy);

    if (current_buffer_pos < RESULT_BUFFER_SIZE - 2) {
        result_buffer[current_buffer_pos++] = ']';
        result_buffer[current_buffer_pos++] = '}';
    } else {
        if (result_buffer[RESULT_BUFFER_SIZE - 2] == ',') {
             result_buffer[RESULT_BUFFER_SIZE - 2] = ']'; result_buffer[RESULT_BUFFER_SIZE - 1] = '}';
        } else if (result_buffer[RESULT_BUFFER_SIZE - 3] == ',') {
             result_buffer[RESULT_BUFFER_SIZE - 3] = ']'; result_buffer[RESULT_BUFFER_SIZE - 2] = '}';
             result_buffer[RESULT_BUFFER_SIZE - 1] = '\0';
        } else {
            char* end_ptr = result_buffer + RESULT_BUFFER_SIZE - 1 - 2;
            if (end_ptr > result_buffer) strncpy(end_ptr, "]}", 2);
        }
        LOG_WARN("PasswordAnalyzer: Result buffer potentially truncated while finalizing JSON.");
        current_buffer_pos = RESULT_BUFFER_SIZE -1;
        result_buffer[RESULT_BUFFER_SIZE - 1] = '\0';
    }
    result_buffer[current_buffer_pos] = '\0';

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("PasswordAnalyzer: Failed to allocate output chunk.");
        return NULL;
    }
    output_chunk->data = strdup(result_buffer);
    if (!output_chunk->data) {
        LOG_ERROR("PasswordAnalyzer: Failed to strdup result_buffer.");
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = strlen(output_chunk->data);
    output_chunk->data_type = CHUNK_TYPE_UTF8_TEXT;

    if (input_chunk->source_id) {
        output_chunk->source_id = strdup(input_chunk->source_id);
        if (!output_chunk->source_id) {
            LOG_WARN("PasswordAnalyzer: Failed to duplicate source_id.");
        }
    } else {
        output_chunk->source_id = NULL;
    }

    LOG_INFO("PasswordAnalyzer: Processing complete.");
    return output_chunk;
}
