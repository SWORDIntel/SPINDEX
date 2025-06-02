#define _GNU_SOURCE // For strdup
#include "txt_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <ctype.h> // Not directly used here, but good for text processing

#include "plugin_interface.h" // For ProcessedDataChunk, PluginInfo
#include "data_types.h"       // For SpindexPluginType, ChunkDataType
#include "utils.h"            // For logging

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "TXTParser";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Parses preprocessed text into a structured format (simple JSON).";

// Basic JSON string escaping (simplified - only handles a few common cases)
static char* escape_json_string(const char* input) {
    if (!input) return strdup("");

    size_t input_len = strlen(input);
    // Estimate new length: start with current length + some buffer.
    // This is a common source of bugs if not generous enough.
    size_t new_len_estimate = input_len * 2 + 1; // Double for worst-case all-escaped + null
    if (new_len_estimate < input_len) { // Overflow check
        LOG_ERROR("[%s] escape_json_string: Length overflow for escape estimation.", G_PLUGIN_NAME);
        return NULL;
    }

    char* escaped = (char*)malloc(new_len_estimate);
    if (!escaped) {
        LOG_ERROR("[%s] escape_json_string: Failed to allocate memory (size %zu).", G_PLUGIN_NAME, new_len_estimate);
        return NULL;
    }

    char* out_ptr = escaped;
    const char* end_of_estimate = escaped + new_len_estimate - 1; // Leave space for null terminator

    for (const char* in_ptr = input; *in_ptr; ++in_ptr) {
        if (out_ptr >= end_of_estimate - 2) { // Need up to 2 chars for escape seq (\ + char) + null
            LOG_WARN("[%s] escape_json_string: Approaching buffer limit, may truncate.", G_PLUGIN_NAME);
            // In a robust implementation, realloc here. For now, truncate if too close.
             size_t current_offset = out_ptr - escaped;
             new_len_estimate = new_len_estimate * 2;
             if (new_len_estimate < current_offset) { // Overflow
                LOG_ERROR("[%s] escape_json_string: Length overflow during realloc estimation.", G_PLUGIN_NAME);
                free(escaped); return NULL;
             }
             char* temp_escaped = (char*)realloc(escaped, new_len_estimate);
             if(!temp_escaped){
                LOG_ERROR("[%s] escape_json_string: Failed to reallocate memory.", G_PLUGIN_NAME);
                free(escaped); return NULL;
             }
             escaped = temp_escaped;
             out_ptr = escaped + current_offset;
             end_of_estimate = escaped + new_len_estimate - 1;
        }

        switch (*in_ptr) {
            case '\\': case '"': *out_ptr++ = '\\'; *out_ptr++ = *in_ptr; break;
            case '\b': *out_ptr++ = '\\'; *out_ptr++ = 'b'; break;
            case '\f': *out_ptr++ = '\\'; *out_ptr++ = 'f'; break;
            case '\n': *out_ptr++ = '\\'; *out_ptr++ = 'n'; break;
            case '\r': *out_ptr++ = '\\'; *out_ptr++ = 'r'; break;
            case '\t': *out_ptr++ = '\\'; *out_ptr++ = 't'; break;
            default:
                if ((unsigned char)*in_ptr < 0x20) { // ASCII Control characters
                    // Represent as unicode escape \uXXXX
                    if (out_ptr >= end_of_estimate - 6) { /* space for \uXXXX */
                        // Simplified: skip if not enough space (robust would realloc)
                        *out_ptr++ = '?'; // Placeholder for unescaped control char
                        LOG_WARN("[%s] escape_json_string: Not enough space for unicode escape, using '?'.", G_PLUGIN_NAME);
                        break;
                    }
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

int spindex_plugin_init() { // Matches current plugin_loader
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0;
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk* input_chunk) { // Matches current plugin_loader
    if (!input_chunk || !input_chunk->data) {
        LOG_ERROR("[%s] Received null input chunk or data.", G_PLUGIN_NAME);
        return NULL;
    }
    LOG_DEBUG("[%s] Processing data for source_id: %s", G_PLUGIN_NAME, input_chunk->source_id ? input_chunk->source_id : "N/A");
    LOG_DEBUG("[%s] Input data type: %d", G_PLUGIN_NAME, input_chunk->data_type);


    char* escaped_content = escape_json_string((char*)input_chunk->data);
    if (!escaped_content) {
        LOG_ERROR("[%s] Failed to escape input data for JSON.", G_PLUGIN_NAME);
        return NULL;
    }

    const char* json_format_template = "{\"text_content\":\"%s\"}";
    // Calculate length: template length (excluding %s) + content length + null terminator
    size_t template_len = strlen(json_format_template) - 2; // -2 for %s
    size_t content_len = strlen(escaped_content);
    size_t json_len = template_len + content_len + 1;

    char* json_output = (char*)malloc(json_len);
    if (!json_output) {
        LOG_ERROR("[%s] Failed to allocate memory for JSON output (size %zu).", G_PLUGIN_NAME, json_len);
        free(escaped_content);
        return NULL;
    }
    snprintf(json_output, json_len, json_format_template, escaped_content);
    free(escaped_content);

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate output chunk.", G_PLUGIN_NAME);
        free(json_output);
        return NULL;
    }

    output_chunk->source_id = input_chunk->source_id ? strdup(input_chunk->source_id) : NULL;
    if (input_chunk->source_id && !output_chunk->source_id) {
        LOG_WARN("[%s] Failed to duplicate source_id.", G_PLUGIN_NAME);
    }

    output_chunk->data = (void*)json_output;
    output_chunk->length = strlen(json_output); // Recalculate, snprintf might write less than json_len
    output_chunk->data_type = CHUNK_TYPE_TEXT_PARSED;

    LOG_DEBUG("[%s] Finished processing source_id: %s. Output JSON length: %zu",
              G_PLUGIN_NAME, output_chunk->source_id ? output_chunk->source_id : "N/A", output_chunk->length);
    return output_chunk;
}

void spindex_plugin_shutdown() { // Matches current plugin_loader
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}
