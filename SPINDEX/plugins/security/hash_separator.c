#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h" // For LOG_INFO, LOG_ERROR, etc.

#define RESULT_BUFFER_SIZE 8192

// --- Forward declarations for plugin functions ---
void hash_separator_get_info(PluginInfo *info);
int hash_separator_init();
void hash_separator_shutdown();
ProcessedDataChunk* hash_separator_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
void spindex_plugin_get_info(PluginInfo *info) {
    hash_separator_get_info(info);
}

int spindex_plugin_init() {
    return hash_separator_init();
}

void spindex_plugin_shutdown() {
    hash_separator_shutdown();
}

ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return hash_separator_process_data(input_chunk);
}

// --- Plugin Implementation ---

void hash_separator_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "Hash Separator", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    strncpy(info->version, "0.1", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    info->type = SPINDEX_PLUGIN_TYPE_SECURITY;
    strncpy(info->description, "Identifies potential cryptographic hashes in text.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int hash_separator_init() {
    LOG_INFO("Hash Separator plugin initialized.");
    return 0;
}

void hash_separator_shutdown() {
    LOG_INFO("Hash Separator plugin shutting down.");
}

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static const char* get_hash_type(const char* token) {
    if (!token) return NULL;
    size_t len = strlen(token);

    bool all_hex = true;
    for (size_t i = 0; i < len; ++i) {
        if (!is_hex_char(token[i])) {
            all_hex = false;
            break;
        }
    }

    if (!all_hex) return NULL;

    if (len == 32) return "MD5";
    if (len == 40) return "SHA1";
    if (len == 64) return "SHA256";
    if (len == 128) return "SHA512";

    return NULL;
}

ProcessedDataChunk* hash_separator_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("HashSeparator: Input chunk or its data is NULL or empty.");
        return NULL;
    }

    char* input_copy = strdup(input_chunk->data);
    if (!input_copy) {
        LOG_ERROR("HashSeparator: Failed to strdup input data.");
        return NULL;
    }

    char result_buffer[RESULT_BUFFER_SIZE] = {0};
    int current_buffer_pos = 0;
    current_buffer_pos += snprintf(result_buffer, RESULT_BUFFER_SIZE, "{\"found_hashes\": [");
    bool first_hash = true;

    char* saveptr;
    const char* delimiters = " \t\n\r,:;()[]{}<>"; // Common delimiters
    char* token = strtok_r(input_copy, delimiters, &saveptr);

    while (token != NULL) {
        const char* hash_type = get_hash_type(token);
        if (hash_type) {
            if (!first_hash) {
                if (current_buffer_pos < RESULT_BUFFER_SIZE - 1) {
                    result_buffer[current_buffer_pos++] = ',';
                } else {
                    LOG_WARN("HashSeparator: Buffer full before adding comma for hash object.");
                    break;
                }
            }
            first_hash = false;

            int written = snprintf(result_buffer + current_buffer_pos,
                                   RESULT_BUFFER_SIZE - current_buffer_pos,
                                   "{\"type\": \"%s\", \"hash_value\": \"%s\"}",
                                   hash_type, token);
            if (written < 0 || current_buffer_pos + written >= RESULT_BUFFER_SIZE -1) {
                LOG_WARN("HashSeparator: Buffer truncated while writing hash object for token: %s", token);
                if(current_buffer_pos > 0 && result_buffer[current_buffer_pos-1] == ',') { // Attempt to remove trailing comma
                    current_buffer_pos--;
                    result_buffer[current_buffer_pos] = '\0';
                }
                break;
            }
            current_buffer_pos += written;
            LOG_DEBUG("HashSeparator: Found %s hash: %s", hash_type, token);
        }
        token = strtok_r(NULL, delimiters, &saveptr);
    }
    free(input_copy);

    if (current_buffer_pos < RESULT_BUFFER_SIZE - 2) {
        result_buffer[current_buffer_pos++] = ']';
        result_buffer[current_buffer_pos++] = '}';
    } else {
        // Ensure the JSON is somewhat valid even if truncated near the end.
        // If buffer was full and last char was a comma, try to overwrite it.
        if (result_buffer[RESULT_BUFFER_SIZE - 2] == ',') {
             result_buffer[RESULT_BUFFER_SIZE - 2] = ']';
             result_buffer[RESULT_BUFFER_SIZE - 1] = '}';
        } else if (result_buffer[RESULT_BUFFER_SIZE - 3] == ',') { // if comma was before "]}"
             result_buffer[RESULT_BUFFER_SIZE - 3] = ']';
             result_buffer[RESULT_BUFFER_SIZE - 2] = '}';
             result_buffer[RESULT_BUFFER_SIZE - 1] = '\0'; // Null terminate earlier
        } else { // Just try to close it
            char* end_ptr = result_buffer + RESULT_BUFFER_SIZE - 1 - 2; // space for "]}"
            if (end_ptr > result_buffer) { // Check if there's space
                strncpy(end_ptr, "]}", 2);
            }
        }
        LOG_WARN("HashSeparator: Result buffer potentially truncated while finalizing JSON.");
        current_buffer_pos = RESULT_BUFFER_SIZE -1; // Mark as full
        result_buffer[RESULT_BUFFER_SIZE - 1] = '\0';
    }

    result_buffer[current_buffer_pos] = '\0'; // Ensure null termination

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("HashSeparator: Failed to allocate output chunk.");
        return NULL;
    }

    output_chunk->data = strdup(result_buffer);
    if (!output_chunk->data) {
        LOG_ERROR("HashSeparator: Failed to strdup result_buffer for output chunk.");
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = strlen(output_chunk->data); // Use strlen on the final strduped data
    output_chunk->data_type = CHUNK_TYPE_UTF8_TEXT; // Output is JSON text

    if (input_chunk->source_id) {
        output_chunk->source_id = strdup(input_chunk->source_id);
        if (!output_chunk->source_id) {
            LOG_WARN("HashSeparator: Failed to duplicate source_id.");
        }
    } else {
        output_chunk->source_id = NULL;
    }

    LOG_INFO("HashSeparator: Processing complete. Found hashes formatted in JSON.");
    return output_chunk;
}
