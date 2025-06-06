#define _GNU_SOURCE // For strdup if used in future
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h> // For isspace

#include "sqlite_preprocessor.h"
#include "../../include/utils.h"
#include "../../include/data_types.h"

static const char* G_PLUGIN_NAME = "SQLitePreprocessor";
static const char* G_PLUGIN_VERSION = "0.2.0";
static const char* G_PLUGIN_DESCRIPTION = "Preprocesses text from SQLite DB dumps.";

void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
        info->type = SPINDEX_PLUGIN_TYPE_PREPROCESSOR;
        strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
}

int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0;
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk) {
    LOG_INFO("[%s v%s] Processing data...", G_PLUGIN_NAME, G_PLUGIN_VERSION);

    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("[%s] Input chunk is NULL, has no data, or length is zero.", G_PLUGIN_NAME);
        return NULL;
    }

    if (input_chunk->data_type != CHUNK_TYPE_TEXT_PARSED) {
        LOG_ERROR("[%s] Expected CHUNK_TYPE_TEXT_PARSED, but received type %d.", G_PLUGIN_NAME, input_chunk->data_type);
        return NULL;
    }

    char *original_text = (char*)input_chunk->data;
    size_t original_length = input_chunk->length;

    char *cleaned_text = (char*)malloc(original_length + 1);
    if (!cleaned_text) {
        LOG_ERROR("[%s] Failed to allocate memory for cleaned text.", G_PLUGIN_NAME);
        return NULL;
    }

    size_t current_cleaned_idx = 0;
    int in_whitespace_sequence = 0;

    size_t start_idx = 0;
    while (start_idx < original_length && isspace((unsigned char)original_text[start_idx])) {
        start_idx++;
    }

    for (size_t i = start_idx; i < original_length; ++i) {
        if (original_text[i] == '\n') {
            // Trim trailing spaces before this newline
            if (current_cleaned_idx > 0 && cleaned_text[current_cleaned_idx - 1] == ' ') {
                 current_cleaned_idx--;
            }
            cleaned_text[current_cleaned_idx++] = '\n';
            in_whitespace_sequence = 1;
        } else if (isspace((unsigned char)original_text[i])) {
            if (!in_whitespace_sequence) {
                cleaned_text[current_cleaned_idx++] = ' ';
                in_whitespace_sequence = 1;
            }
        } else {
            cleaned_text[current_cleaned_idx++] = original_text[i];
            in_whitespace_sequence = 0;
        }
    }

    // Trim trailing spaces and the very last newline if present
    while (current_cleaned_idx > 0) {
        if (cleaned_text[current_cleaned_idx - 1] == ' ') {
            current_cleaned_idx--;
        } else if (cleaned_text[current_cleaned_idx - 1] == '\n') {
            // If it's the last char, trim it. Otherwise, keep internal newlines.
            if (current_cleaned_idx == original_length || (current_cleaned_idx > 0 && original_text[current_cleaned_idx] == '\0') ) { // Heuristic: is it the very end?
                 current_cleaned_idx--; // Trim final newline
            }
            break;
        } else {
            break;
        }
    }
     // Simpler final newline trim, consistent with other preprocessors
    if (current_cleaned_idx > 0 && cleaned_text[current_cleaned_idx - 1] == '\n') {
        current_cleaned_idx--;
    }


    cleaned_text[current_cleaned_idx] = '\0';

    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate memory for output chunk.", G_PLUGIN_NAME);
        free(cleaned_text);
        return NULL;
    }
    memset(output_chunk, 0, sizeof(ProcessedDataChunk));

    output_chunk->data = (unsigned char*)cleaned_text;
    output_chunk->length = current_cleaned_idx;
    output_chunk->data_type = CHUNK_TYPE_TEXT_PARSED;

    char source_id_buf[512];
    if (input_chunk->source_id) {
        snprintf(source_id_buf, sizeof(source_id_buf), "%s_preprocessed_sqlite", input_chunk->source_id);
    } else {
        snprintf(source_id_buf, sizeof(source_id_buf), "unknown_preprocessed_sqlite_by_%s", G_PLUGIN_NAME);
    }
    output_chunk->source_id = strdup(source_id_buf);
    if (!output_chunk->source_id) {
        LOG_WARN("[%s] Failed to allocate memory for output source_id string. Continuing without it.", G_PLUGIN_NAME);
    }

    LOG_INFO("[%s] Successfully preprocessed text. Original length: %zu, Cleaned length: %zu", G_PLUGIN_NAME, original_length, output_chunk->length);
    return output_chunk;
}
