#define _GNU_SOURCE // For strdup, strtok_r
#include "csv_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace

#include "plugin_interface.h"
#include "data_types.h"
#include "utils.h"

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "CSVParser";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses CSV data into a JSON array of objects.";

// Helper function to trim leading/trailing whitespace from a string (in-place)
static char* trim_whitespace_csv(char *str) {
    if (!str) return NULL;
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}

// Basic JSON string escaping
static char* escape_json_string_csv(const char* input) {
    if (!input) return strdup("");

    size_t input_len = strlen(input);
    size_t new_len_estimate = input_len * 2 + 7; // Base for growth + \uXXXX escapes
    if (new_len_estimate < input_len) {
        LOG_ERROR("[%s] escape_json_string_csv: Length overflow for escape estimation.", G_PLUGIN_NAME);
        return NULL;
    }

    char* escaped = (char*)malloc(new_len_estimate);
    if (!escaped) {
        LOG_ERROR("[%s] escape_json_string_csv: malloc failed (size %zu)", G_PLUGIN_NAME, new_len_estimate);
        return NULL;
    }

    char* out_ptr = escaped;
    const char* end_of_estimate = escaped + new_len_estimate - 1;

    for (const char* in_ptr = input; *in_ptr; ++in_ptr) {
        if (out_ptr >= end_of_estimate - 6) { // Need up to 6 chars for \uXXXX, or 2 for \X
            size_t current_offset = out_ptr - escaped;
            new_len_estimate = new_len_estimate * 2 + 6; // Grow buffer
             if (new_len_estimate < current_offset) {
                LOG_ERROR("[%s] escape_json_string_csv: Length overflow during realloc estimation.", G_PLUGIN_NAME);
                free(escaped); return NULL;
             }
            char* temp_escaped = (char*)realloc(escaped, new_len_estimate);
            if(!temp_escaped){
                LOG_ERROR("[%s] escape_json_string_csv: Failed to reallocate memory.", G_PLUGIN_NAME);
                free(escaped); return NULL;
            }
            escaped = temp_escaped;
            out_ptr = escaped + current_offset;
            end_of_estimate = escaped + new_len_estimate - 1;
        }

        switch (*in_ptr) {
            case '\\': *out_ptr++ = '\\'; *out_ptr++ = '\\'; break;
            case '"':  *out_ptr++ = '\\'; *out_ptr++ = '"';  break;
            case '\b': *out_ptr++ = '\\'; *out_ptr++ = 'b';  break;
            case '\f': *out_ptr++ = '\\'; *out_ptr++ = 'f';  break;
            case '\n': *out_ptr++ = '\\'; *out_ptr++ = 'n';  break;
            case '\r': *out_ptr++ = '\\'; *out_ptr++ = 'r';  break;
            case '\t': *out_ptr++ = '\\'; *out_ptr++ = 't';  break;
            default:
                if ((unsigned char)*in_ptr < 0x20) {
                    sprintf(out_ptr, "\\u%04x", (unsigned char)*in_ptr);
                    out_ptr += 6;
                } else {
                    *out_ptr++ = *in_ptr;
                }
                break;
        }
    }
    *out_ptr = '\0';
    return escaped;
}

// Helper function to safely append to json_buffer, reallocating if necessary
static char* append_to_json_buffer(char* buffer, size_t* buffer_size_ptr, size_t* current_len_ptr, const char* str_to_append) {
    if (!str_to_append) return buffer; // Nothing to append
    size_t append_len = strlen(str_to_append);
    if (append_len == 0) return buffer; // Nothing to append

    if (*current_len_ptr + append_len + 1 > *buffer_size_ptr) { // +1 for null terminator
        size_t new_size = *buffer_size_ptr * 2;
        if (new_size < *current_len_ptr + append_len + 1) {
            new_size = *current_len_ptr + append_len + 1 + 256; // Ensure enough, plus some headroom
        }
        if (new_size < *buffer_size_ptr) { // Check for overflow if new_size wrapped around
            LOG_ERROR("[%s] append_to_json_buffer: new_size calculation resulted in overflow.", G_PLUGIN_NAME);
            return NULL; // Indicate critical error, caller must free original buffer
        }
        char* new_buffer_ptr = (char*)realloc(buffer, new_size);
        if (!new_buffer_ptr) {
            LOG_ERROR("[%s] Failed to realloc JSON buffer in append_to_json_buffer (current_size: %zu, new_size: %zu). Original buffer at %p is still valid but likely too small.", G_PLUGIN_NAME, *buffer_size_ptr, new_size, (void*)buffer);
            return NULL; // Caller must free original buffer
        }
        buffer = new_buffer_ptr; // Update the buffer pointer for this scope and for return
        *buffer_size_ptr = new_size; // Update the caller's buffer_size variable
    }
    // Using memcpy is safer than strcat if we know the lengths
    memcpy(buffer + *current_len_ptr, str_to_append, append_len);
    *current_len_ptr += append_len;
    buffer[*current_len_ptr] = '\0'; // Ensure null termination
    return buffer;
}


// --- Plugin API Functions ---
void spindex_plugin_get_info(PluginInfo* info) {
    if (!info) return;
    strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';

    info->type = SPINDEX_PLUGIN_TYPE_PARSER;

    strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0;
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk* input_chunk) {
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("[%s] Received null input chunk or data.", G_PLUGIN_NAME);
        return NULL;
    }
    LOG_DEBUG("[%s] Processing data for source_id: %s", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "N/A");

    char* csv_data_copy = strdup((char*)input_chunk->data);
    if (!csv_data_copy) {
        LOG_ERROR("[%s] Failed to copy input data.", G_PLUGIN_NAME);
        return NULL;
    }

    char* line_saveptr;
    char* line = strtok_r(csv_data_copy, "\r\n", &line_saveptr); // Handle both \n and \r\n

    char** headers = NULL;
    int header_count = 0;

    if (line) {
        char* header_token_saveptr;
        char* token = strtok_r(line, ",", &header_token_saveptr);
        while (token) {
            char** temp_headers = (char**)realloc(headers, (header_count + 1) * sizeof(char*));
            if (!temp_headers) {
                LOG_ERROR("[%s] Failed to realloc for headers.", G_PLUGIN_NAME);
                if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); }
                free(csv_data_copy); return NULL;
            }
            headers = temp_headers;
            // Whitespace is trimmed by trim_whitespace_csv call
            char* trimmed_token = trim_whitespace_csv(token);
            headers[header_count] = strdup(trimmed_token);
            if (!headers[header_count]) {
                 LOG_ERROR("[%s] Failed to strdup trimmed header token.", G_PLUGIN_NAME);
                 if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); }
                 free(csv_data_copy); return NULL;
            }
            header_count++;
            token = strtok_r(NULL, ",", &header_token_saveptr);
        }
        line = strtok_r(NULL, "\r\n", &line_saveptr);
    }

    if (header_count == 0) { // No headers or empty first line
        LOG_WARN("[%s] Empty or no header line in CSV for source_id: %s", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "N/A");
        if (headers) free(headers);
        free(csv_data_copy);
        char* empty_json_array = strdup("[]");
        if(!empty_json_array) { LOG_ERROR("[%s] strdup failed for empty json array", G_PLUGIN_NAME); return NULL;}

        ProcessedDataChunk* out = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
        if(!out) { free(empty_json_array); LOG_ERROR("[%s] Malloc failed for empty output chunk", G_PLUGIN_NAME); return NULL; }

        out->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
        if (input_chunk->source_id && !out->source_id) {
            LOG_WARN("[%s] Strdup failed for source_id (empty CSV)", G_PLUGIN_NAME);
        }
        out->data = (void*)empty_json_array;
        out->length = strlen(empty_json_array);
        out->data_type = CHUNK_TYPE_CSV_PARSED;
        return out;
    }

    size_t buffer_size = 1024; // Initial size
    char* json_buffer = (char*)malloc(buffer_size);
    if (!json_buffer) {
        LOG_ERROR("[%s] Failed to allocate initial JSON buffer.", G_PLUGIN_NAME);
        if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); }
        free(csv_data_copy);
        return NULL;
    }
    json_buffer[0] = '\0'; // Initialize as empty string for append_to_json_buffer
    size_t current_len = 0;
    char* new_ptr = NULL;

    new_ptr = append_to_json_buffer(json_buffer, &buffer_size, &current_len, "[");
    if (!new_ptr) { if (json_buffer) free(json_buffer); json_buffer = NULL; goto csv_parser_cleanup_final; }
    json_buffer = new_ptr;

    int first_row = 1;

    while (line) {
        if (strlen(line) == 0) { // Skip empty lines
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }
        if (!first_row) {
            new_ptr = append_to_json_buffer(json_buffer, &buffer_size, &current_len, ",");
            if (!new_ptr) { if (json_buffer) free(json_buffer); json_buffer = NULL; goto csv_parser_cleanup_final; }
            json_buffer = new_ptr;
        }
        first_row = 0;

        new_ptr = append_to_json_buffer(json_buffer, &buffer_size, &current_len, "{");
        if (!new_ptr) { if (json_buffer) free(json_buffer); json_buffer = NULL; goto csv_parser_cleanup_final; }
        json_buffer = new_ptr;

        char* field_token_saveptr;
        char* field = strtok_r(line, ",", &field_token_saveptr);
        int field_idx = 0;
        int first_field_in_row = 1;

        while (field_idx < header_count) {
            if (!first_field_in_row) {
                new_ptr = append_to_json_buffer(json_buffer, &buffer_size, &current_len, ",");
                if (!new_ptr) { if (json_buffer) free(json_buffer); json_buffer = NULL; goto csv_parser_cleanup_final; }
                json_buffer = new_ptr;
            }
            first_field_in_row = 0;

            char* current_field_value = field ? trim_whitespace_csv(field) : "";
            char* escaped_header = escape_json_string_csv(headers[field_idx]);
            char* escaped_field = escape_json_string_csv(current_field_value);

            if(!escaped_header || !escaped_field) {
                LOG_ERROR("[%s] Failed to escape header/field.", G_PLUGIN_NAME);
                if(escaped_header) free(escaped_header);
                if(escaped_field) free(escaped_field);
                if (json_buffer) free(json_buffer); json_buffer = NULL; // Free current json_buffer before exiting loop
                goto csv_parser_cleanup_final;
            }

            size_t kv_pair_len_estimate = strlen(escaped_header) + strlen(escaped_field) + 5;
            if (current_len + kv_pair_len_estimate > buffer_size) {
                 size_t new_b_size = buffer_size * 2;
                 if (new_b_size < current_len + kv_pair_len_estimate) {
                    new_b_size = current_len + kv_pair_len_estimate + 256;
                 }
                 if (new_b_size < buffer_size) {
                     LOG_ERROR("[%s] Buffer size overflow for KV pair", G_PLUGIN_NAME);
                     free(escaped_header); free(escaped_field);
                     if (json_buffer) free(json_buffer); json_buffer = NULL;
                     goto csv_parser_cleanup_final;
                 }

                 char* new_alloc_buffer = (char*)realloc(json_buffer, new_b_size);
                 if (!new_alloc_buffer) {
                    LOG_ERROR("[%s] Failed to realloc JSON buffer for KV pair. Original buffer at %p still valid.", G_PLUGIN_NAME, (void*)json_buffer);
                    free(escaped_header);
                    free(escaped_field);
                    if (json_buffer) free(json_buffer); json_buffer = NULL; // Free original json_buffer
                    goto csv_parser_cleanup_final;
                 }
                 json_buffer = new_alloc_buffer;
                 buffer_size = new_b_size;
            }

            int written = snprintf(json_buffer + current_len, buffer_size - current_len, "\"%s\":\"%s\"", escaped_header, escaped_field);
            if (written < 0 || (size_t)written >= buffer_size - current_len) {
                 LOG_ERROR("[%s] snprintf failed or truncated for KV pair.", G_PLUGIN_NAME);
                 free(escaped_header); free(escaped_field);
                 if (json_buffer) free(json_buffer); json_buffer = NULL;
                 goto csv_parser_cleanup_final;
            }
            current_len += written;

            free(escaped_header);
            free(escaped_field);
            // No need to NULL escaped_header/field here as we goto final cleanup on error

            if(field) field = strtok_r(NULL, ",", &field_token_saveptr);
            field_idx++;
        } // End of while (field_idx < header_count)

        new_ptr = append_to_json_buffer(json_buffer, &buffer_size, &current_len, "}");
        if (!new_ptr) { if (json_buffer) free(json_buffer); json_buffer = NULL; goto csv_parser_cleanup_final; }
        json_buffer = new_ptr;
        line = strtok_r(NULL, "\r\n", &line_saveptr);
    } // End of while (line)

    new_ptr = append_to_json_buffer(json_buffer, &buffer_size, &current_len, "]");
    if (!new_ptr) { if (json_buffer) free(json_buffer); json_buffer = NULL; goto csv_parser_cleanup_final; }
    json_buffer = new_ptr;

csv_parser_cleanup_final: // Unified cleanup point
    if (csv_data_copy) {
        free(csv_data_copy);
        // csv_data_copy = NULL; // Not strictly needed as function scope ends
    }
    if (headers) {
        for (int i = 0; i < header_count; ++i) free(headers[i]);
        free(headers);
        // headers = NULL; // Not strictly needed
    }

    if (!json_buffer) {
        LOG_DEBUG("[%s] CSV parsing resulted in NULL json_buffer, likely due to error.", G_PLUGIN_NAME);
        return NULL;
    }

    // Success path:
    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate output chunk.", G_PLUGIN_NAME);
        free(json_buffer); // json_buffer is valid here
        return NULL;
    }
    output_chunk->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
    if(input_chunk->source_id && !output_chunk->source_id) { LOG_WARN("[%s] strdup source_id failed for output", G_PLUGIN_NAME); }

    output_chunk->data = (void*)json_buffer; // json_buffer is now owned by output_chunk
    output_chunk->length = current_len;
    output_chunk->data_type = CHUNK_TYPE_CSV_PARSED;

    LOG_DEBUG("[%s] Finished processing source_id: %s. Output JSON length: %zu",
              G_PLUGIN_NAME, output_chunk->source_id ? output_chunk->source_id : "N/A", output_chunk->length);
    return output_chunk;
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // No global state to cleanup in this plugin
}
// Plugin dependency management (stub - no dependencies)
SpindexPluginDependency* spindex_plugin_get_dependencies(int* count) {
    if (count) *count = 0;
    return NULL;  // This plugin has no dependencies
}

void spindex_plugin_free_dependencies_list(SpindexPluginDependency* dependencies, int count) {
    // Nothing to free - we returned NULL
    (void)dependencies;
    (void)count;
}

