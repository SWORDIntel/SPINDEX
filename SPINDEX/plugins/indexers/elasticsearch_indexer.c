#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elasticsearch_indexer.h"
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/utils.h"
#include "../../include/elasticsearch_client.h"
#include "../../include/plugin_loader.h"
#include "../../include/avx_search_params.h"

static const char* G_PLUGIN_NAME = "ElasticsearchIndexer";
static const char* G_PLUGIN_VERSION = "0.2.0";
static const char* G_PLUGIN_DESCRIPTION = "Indexes data into Elasticsearch, with AVX-512 phrase extraction.";

typedef struct {
    ElasticsearchClient* es_client;
    char* index_name;
    unsigned long doc_counter;
    SpindexPlugin* avx_search_plugin;
    char* phrases_to_extract;
} ElasticsearchIndexerData;

static ElasticsearchIndexerData* S_GLOBAL_INDEXER_DATA = NULL;

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
    S_GLOBAL_INDEXER_DATA = calloc(1, sizeof(ElasticsearchIndexerData));
    if (!S_GLOBAL_INDEXER_DATA) {
        LOG_ERROR("Failed to allocate memory for indexer data");
        return -1;
    }

    S_GLOBAL_INDEXER_DATA->es_client = es_create_client("http://localhost:9200");
    if (!S_GLOBAL_INDEXER_DATA->es_client) {
        LOG_ERROR("Failed to create Elasticsearch client");
        free(S_GLOBAL_INDEXER_DATA);
        S_GLOBAL_INDEXER_DATA = NULL;
        return -1;
    }

    S_GLOBAL_INDEXER_DATA->index_name = strdup("spindex_default");
    S_GLOBAL_INDEXER_DATA->avx_search_plugin = spindex_load_plugin("plugins/search_algorithms/avx512_phrase_search.so");
    if (!S_GLOBAL_INDEXER_DATA->avx_search_plugin) {
        LOG_ERROR("Failed to load AVX search plugin");
        es_destroy_client(S_GLOBAL_INDEXER_DATA->es_client);
        free(S_GLOBAL_INDEXER_DATA->index_name);
        free(S_GLOBAL_INDEXER_DATA);
        S_GLOBAL_INDEXER_DATA = NULL;
        return -1;
    }

    // In a real application, this would be loaded from a config file
    S_GLOBAL_INDEXER_DATA->phrases_to_extract = strdup("hello world\nanother phrase");

    LOG_INFO("[%s v%s] Initialized.", G_PLUGIN_NAME, G_PLUGIN_VERSION);
    return 0;
}

void spindex_plugin_shutdown() {
    if (S_GLOBAL_INDEXER_DATA) {
        es_destroy_client(S_GLOBAL_INDEXER_DATA->es_client);
        free(S_GLOBAL_INDEXER_DATA->index_name);
        spindex_unload_plugin(S_GLOBAL_INDEXER_DATA->avx_search_plugin);
        free(S_GLOBAL_INDEXER_DATA->phrases_to_extract);
        free(S_GLOBAL_INDEXER_DATA);
        S_GLOBAL_INDEXER_DATA = NULL;
    }
    LOG_INFO("[%s v%s] Shutdown.", G_PLUGIN_NAME, G_PLUGIN_VERSION);
}

ProcessedDataChunk* spindex_plugin_process_data(const ProcessedDataChunk *input_chunk) {
    if (!S_GLOBAL_INDEXER_DATA || !S_GLOBAL_INDEXER_DATA->es_client) {
        LOG_ERROR("[%s] Indexer not properly initialized or ES client is missing.", G_PLUGIN_NAME);
        return NULL;
    }

    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        LOG_ERROR("[%s] Input chunk or data is NULL or empty.", G_PLUGIN_NAME);
        return NULL;
    }

    char* extracted_phrases = NULL;
    if (S_GLOBAL_INDEXER_DATA->avx_search_plugin && S_GLOBAL_INDEXER_DATA->phrases_to_extract) {
        // AVX search integration disabled for now - needs proper plugin API
        // TODO: Implement proper plugin function call interface
        /*
        AvxSearchParams params = {
            .text_to_search = input_chunk,
            .phrases_to_find = &(ProcessedDataChunk){
                .data = (unsigned char*)S_GLOBAL_INDEXER_DATA->phrases_to_extract,
                .length = strlen(S_GLOBAL_INDEXER_DATA->phrases_to_extract),
                .data_type = CHUNK_TYPE_UTF8_TEXT
            },
            .case_sensitive = false,
            .find_all_occurrences = true,
            .max_hits_per_doc = 0
        };
        */
    }

    char doc_id_buf[64];
    snprintf(doc_id_buf, sizeof(doc_id_buf), "doc_%lu", S_GLOBAL_INDEXER_DATA->doc_counter++);

    char* final_doc;
    if (extracted_phrases) {
        // This is a simplified way to add a field to a JSON object.
        // A robust implementation would use a JSON library like Jansson.
        size_t final_doc_size = input_chunk->length + strlen(extracted_phrases) + 128;
        final_doc = malloc(final_doc_size);
        snprintf(final_doc, final_doc_size, "%.*s, \"extracted_phrases\": \"%s\"}",
                 (int)input_chunk->length - 1, (char*)input_chunk->data, extracted_phrases);
        free(extracted_phrases);
    } else {
        final_doc = (char*)input_chunk->data;
    }

    es_index_document(S_GLOBAL_INDEXER_DATA->es_client,
                                   S_GLOBAL_INDEXER_DATA->index_name,
                                   doc_id_buf,
                                   final_doc);

    if (extracted_phrases) {
        free(final_doc);
    }

    return NULL;
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

