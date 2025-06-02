#define _GNU_SOURCE // For strdup, strtok_r
#include "csv_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugin_interface.h"
#include "data_types.h"
#include "utils.h"

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "CSVParser";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses CSV data into a JSON array of objects.";

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
            case '\b': *out_ptr++ = '\\'; *out_ptr++ = 'b';  break; // Corrected literal
            case '\f': *out_ptr++ = '\\'; *out_ptr++ = 'f';  break; // Corrected literal
            case '\n': *out_ptr++ = '\\'; *out_ptr++ = 'n';  break; // Corrected literal
            case '\r': *out_ptr++ = '\\'; *out_ptr++ = 'r';  break; // Corrected literal
            case '\t': *out_ptr++ = '\\'; *out_ptr++ = 't';  break; // Corrected literal
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
            headers[header_count] = strdup(token); // TODO: trim whitespace from header token
            if (!headers[header_count]) {
                 LOG_ERROR("[%s] Failed to strdup header token.", G_PLUGIN_NAME);
                 if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); /* headers[header_count] was not set */ free(headers); }
                 free(csv_data_copy); return NULL;
            }
            header_count++;
            token = strtok_r(NULL, ",", &header_token_saveptr);
        }
        line = strtok_r(NULL, "\r\n", &line_saveptr);
    }

    if (header_count == 0) { // No headers or empty first line
        LOG_WARN("[%s] Empty or no header line in CSV for source_id: %s", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "N/A");
        if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); } // header_count is 0, loop won't run.
        free(csv_data_copy);
        char* empty_json_array = strdup("[]");
        if(!empty_json_array) { LOG_ERROR("[%s] strdup failed for empty json array", G_PLUGIN_NAME); return NULL;}

        ProcessedDataChunk* out = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
        if(!out) { free(empty_json_array); LOG_ERROR("[%s] Malloc failed for empty output chunk", G_PLUGIN_NAME); return NULL; }

        out->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
        if (input_chunk->source_id && !out->source_id) {
            LOG_WARN("[%s] Strdup failed for source_id (empty CSV)", G_PLUGIN_NAME);
            // Potentially continue without source_id or fail, for now continue
        }
        out->data = (void*)empty_json_array;
        out->length = strlen(empty_json_array);
        out->data_type = CHUNK_TYPE_CSV_PARSED;
        return out;
    }

    size_t buffer_size = 1024;
    char* json_buffer = (char*)malloc(buffer_size);
    if (!json_buffer) {
        LOG_ERROR("[%s] Failed to allocate initial JSON buffer.", G_PLUGIN_NAME);
        if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); }
        free(csv_data_copy);
        return NULL;
    }
    strcpy(json_buffer, "[");
    size_t current_len = 1;
    int first_row = 1;

    while (line) {
        if (strlen(line) == 0) {
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }
        if (!first_row) {
            if (current_len + 2 > buffer_size) { /* TODO: realloc json_buffer */ }
            strcat(json_buffer, ","); current_len++;
        }
        first_row = 0;

        if (current_len + 2 > buffer_size) { /* TODO: realloc json_buffer */ }
        strcat(json_buffer, "{"); current_len++;

        char* field_token_saveptr;
        char* field = strtok_r(line, ",", &field_token_saveptr);
        int field_idx = 0;
        int first_field_in_row = 1;

        while (field_idx < header_count) {
            if (!first_field_in_row) {
                if (current_len + 2 > buffer_size) { /* TODO: realloc json_buffer */ }
                strcat(json_buffer, ","); current_len++;
            }
            first_field_in_row = 0;

            char* current_field_value = field ? field : "";
            char* escaped_header = escape_json_string_csv(headers[field_idx]); // TODO: trim whitespace from headers[field_idx]
            char* escaped_field = escape_json_string_csv(current_field_value); // TODO: trim whitespace from current_field_value

            if(!escaped_header || !escaped_field) {
                LOG_ERROR("[%s] Failed to escape header/field.", G_PLUGIN_NAME);
                if(escaped_header) free(escaped_header);
                if(escaped_field) free(escaped_field);
                if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); }
                free(csv_data_copy); free(json_buffer); return NULL;
            }

            size_t needed = strlen(escaped_header) + strlen(escaped_field) + 6;
            if (current_len + needed > buffer_size) {
                 size_t new_size = buffer_size * 2 > current_len + needed ? buffer_size * 2 : current_len + needed + 256; // + some buffer
                 char* new_buffer = (char*)realloc(json_buffer, new_size);
                 if (!new_buffer) {
                    LOG_ERROR("[%s] Failed to realloc JSON buffer.", G_PLUGIN_NAME);
                    free(escaped_header);
                    free(escaped_field);
                    if (headers) { for(int i=0; i<header_count; ++i) free(headers[i]); free(headers); }
                    free(csv_data_copy);
                    free(json_buffer); // old json_buffer, new_buffer is NULL
                    return NULL;
                 }
                 json_buffer = new_buffer;
                 buffer_size = new_size;
            }
            int written = snprintf(json_buffer + current_len, buffer_size - current_len, "\"%s\":\"%s\"", escaped_header, escaped_field);
            if (written < 0 || (size_t)written >= buffer_size - current_len) { /* error or truncation */ }
            else current_len += written;


            free(escaped_header);
            free(escaped_field);

            if(field) field = strtok_r(NULL, ",", &field_token_saveptr);
            field_idx++;
        }
        if (current_len + 2 > buffer_size) { /* TODO: realloc json_buffer */ }
        strcat(json_buffer, "}"); current_len++;
        line = strtok_r(NULL, "\r\n", &line_saveptr);
    }

    if (current_len + 2 > buffer_size) { /* TODO: realloc json_buffer */ }
    strcat(json_buffer, "]"); current_len++;

    for (int i = 0; i < header_count; ++i) free(headers[i]);
    free(headers);
    free(csv_data_copy);

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate output chunk.", G_PLUGIN_NAME);
        free(json_buffer);
        return NULL;
    }
    output_chunk->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
    if(input_chunk->source_id && !output_chunk->source_id) { LOG_WARN("[%s] strdup source_id failed for output", G_PLUGIN_NAME); }

    output_chunk->data = (void*)json_buffer;
    output_chunk->length = current_len;
    output_chunk->data_type = CHUNK_TYPE_CSV_PARSED;

    LOG_DEBUG("[%s] Finished processing source_id: %s. Output JSON length: %zu",
              G_PLUGIN_NAME, output_chunk->source_id ? output_chunk->source_id : "N/A", output_chunk->length);
    return output_chunk;
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}
