#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elasticsearch_indexer.h" // Own header (minimal)
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"    // For SpindexPluginType, PluginInfo, ProcessedDataChunk etc.
#include "../../include/utils.h"         // For LOG_INFO, LOG_ERROR etc.
#include "../../include/elasticsearch_client.h" // To use the ES client

// --- Plugin Static Information ---
static const char* G_PLUGIN_NAME = "ElasticsearchIndexer";
static const char* G_PLUGIN_VERSION = "0.1.0";
static const char* G_PLUGIN_DESCRIPTION = "Indexes data into Elasticsearch.";

// --- Plugin Data Struct ---
typedef struct {
    ElasticsearchClient* es_client;
    char* index_name; // Store a copy of the index name
    unsigned long doc_counter; // For generating unique doc IDs if needed
} ElasticsearchIndexerData;

// --- File-Scope Static Global for Plugin Data (Workaround) ---
// This is NOT thread-safe and assumes a single instance of this plugin.
// This is a temporary solution due to current plugin_loader limitations.
// static ElasticsearchIndexerData* S_GLOBAL_INDEXER_DATA = NULL; // Temporarily disable for testing

// --- Plugin API Functions ---

void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        strncpy(info->name, G_PLUGIN_NAME, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';

        strncpy(info->version, G_PLUGIN_VERSION, sizeof(info->version) - 1);
        info->version[sizeof(info->version) - 1] = '\0';

        info->type = SPINDEX_PLUGIN_TYPE_INDEXER;

        strncpy(info->description, G_PLUGIN_DESCRIPTION, sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
}

int spindex_plugin_init() {
    LOG_INFO("[%s v%s] Initializing... (Simplified for debugging)", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // Temporarily do not allocate or initialize S_GLOBAL_INDEXER_DATA
    LOG_INFO("[%s v%s] Initialization complete. (Simplified for debugging)", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0; // Success
}

void spindex_plugin_shutdown() {
    LOG_INFO("[%s v%s] Shutting down... (Simplified for debugging)", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    // Temporarily do not deallocate S_GLOBAL_INDEXER_DATA
    LOG_INFO("[%s v%s] Shutdown complete. (Simplified for debugging)", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk) {
    LOG_INFO("[%s v%s] Processing data for indexing... (Simplified for debugging)", G_PLUGIN_NAME, G_PLUGIN_VERSION);

    // if (!S_GLOBAL_INDEXER_DATA || !S_GLOBAL_INDEXER_DATA->es_client) {
    //     LOG_ERROR("[%s] Indexer not properly initialized or ES client is missing.", G_PLUGIN_NAME);
    //     return NULL;
    // }
    // ElasticsearchIndexerData* indexer_data = S_GLOBAL_INDEXER_DATA; // This line was the source of previous error

    LOG_WARN("[%s] Indexing process is currently stubbed out for debugging.", G_PLUGIN_NAME);

    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("[%s] Input chunk or data is NULL or empty.", G_PLUGIN_NAME);
        return NULL;
    }

    // Assuming input_chunk->data is a JSON string.
    // All logic using 'indexer_data' is commented out because S_GLOBAL_INDEXER_DATA is disabled.
    // char doc_id_buf[64];
    // snprintf(doc_id_buf, sizeof(doc_id_buf), "doc_%lu", indexer_data->doc_counter++);

    // LOG_INFO("[%s] Attempting to index document ID: %s to index: %s",
    //          G_PLUGIN_NAME, doc_id_buf, indexer_data->index_name);
    LOG_DEBUG("[%s] Document JSON (first 100 bytes): %.*s", G_PLUGIN_NAME, 100, (char*)input_chunk->data);

    // int result = es_index_document(indexer_data->es_client,
    //                                indexer_data->index_name,
    //                                doc_id_buf,
    //                                (const char*)input_chunk->data);

    // if (result == 0) {
    //     LOG_INFO("[%s] Successfully indexed document ID: %s", G_PLUGIN_NAME, doc_id_buf);
    // } else {
    //     LOG_ERROR("[%s] Failed to index document ID: %s", G_PLUGIN_NAME, doc_id_buf);
    // }

    return NULL;
}
