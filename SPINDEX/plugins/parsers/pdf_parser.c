#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdf_parser.h"
#include "../../include/utils.h"
#include "../../include/data_types.h" // For CHUNK_TYPE_RAW_BYTES

static const char* G_PLUGIN_NAME = "PDFParser";
static const char* G_PLUGIN_VERSION = "0.1.0-stub";
static const char* G_PLUGIN_DESCRIPTION = "Parses PDF files. (Stub Implementation)";

void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';
        info->type = SPINDEX_PLUGIN_TYPE_PARSER;
        strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
}

int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing (stub)...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0;
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down (stub)...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk) {
    LOG_INFO("[%s v%s] Processing data (stub)...", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    if (!input_chunk) {
        LOG_ERROR("[%s] Input chunk is NULL.", G_PLUGIN_NAME);
        return NULL;
    }
    LOG_INFO("[%s] Data is present, but this is a stub. No actual parsing will occur.", G_PLUGIN_NAME);

    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("[%s] Failed to allocate memory for output chunk.", G_PLUGIN_NAME);
        return NULL;
    }
    memcpy(output_chunk, input_chunk, sizeof(ProcessedDataChunk));

    if (input_chunk->data && input_chunk->length > 0) {
        output_chunk->data = malloc(input_chunk->length);
        if (!output_chunk->data) {
            LOG_ERROR("[%s] Failed to allocate memory for output data.", G_PLUGIN_NAME);
            free(output_chunk);
            return NULL;
        }
        memcpy(output_chunk->data, input_chunk->data, input_chunk->length);
    } else {
        output_chunk->data = NULL;
    }

    char source_buf[512];
    if (input_chunk->source_id) {
        snprintf(source_buf, sizeof(source_buf), "%s_stubbed_by_%s", input_chunk->source_id, G_PLUGIN_NAME);
    } else {
        snprintf(source_buf, sizeof(source_buf), "unknown_stubbed_by_%s", G_PLUGIN_NAME);
    }
    output_chunk->source_id = strdup(source_buf);
    if (!output_chunk->source_id) {
         LOG_WARN("[%s] Failed to allocate memory for output source_id string.", G_PLUGIN_NAME);
    }

    output_chunk->data_type = CHUNK_TYPE_RAW_BYTES;
    LOG_INFO("[%s] Returning a copy of the input chunk (stub behavior).", G_PLUGIN_NAME);
    return output_chunk;
}
