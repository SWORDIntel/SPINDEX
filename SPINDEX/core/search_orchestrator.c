#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/search_orchestrator.h"
#include "../include/elasticsearch_client.h" // For ElasticsearchClient (actual client not fully used yet)
#include "../include/avx_search_params.h"   // For AvxSearchParams
#include "../include/utils.h"               // For LOG_INFO, LOG_ERROR, etc.
#include "../include/plugin_interface.h"    // For SpindexPlugin definition
#include "../include/config.h"              // For SpindexConfig
#include "../include/data_types.h"          // For ProcessedDataChunk and CHUNK_TYPE_*

// Initial capacity for SearchResults items array
#define INITIAL_SEARCH_RESULTS_CAPACITY 10

struct SearchOrchestrator {
    SpindexPlugin *all_plugins_list; // Reference to the global list of loaded plugins
    int num_plugins;
    const SpindexConfig *app_config;
    SpindexPlugin *avx_search_plugin; // Cached pointer to the AVX search plugin
    ElasticsearchClient* es_client;   // Placeholder for ES client
};

// Helper function to create a ProcessedDataChunk for text (used for AVX search input)
static ProcessedDataChunk* create_text_chunk(const char* text_content, const char* source_id_prefix, int doc_num) {
    if (!text_content) return NULL;

    ProcessedDataChunk* chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!chunk) {
        LOG_ERROR("Failed to allocate memory for ProcessedDataChunk (text)");
        return NULL;
    }
    chunk->data = (unsigned char*)strdup(text_content);
    if (!chunk->data) {
        LOG_ERROR("Failed to strdup text_content");
        free(chunk);
        return NULL;
    }
    chunk->length = strlen(text_content);

    char source_id_buffer[128];
    snprintf(source_id_buffer, sizeof(source_id_buffer), "%s_doc_%d", source_id_prefix, doc_num);
    chunk->source_id = strdup(source_id_buffer);
    if (!chunk->source_id) {
        LOG_WARN("Failed to strdup source_id for text_chunk");
        // Not fatal, can proceed without source_id or with NULL
    }
    chunk->data_type = CHUNK_TYPE_UTF8_TEXT; // Assuming text is UTF-8
    return chunk;
}


SearchOrchestrator* search_orchestrator_create(
    SpindexPlugin *all_loaded_plugins,
    int num_all_plugins,
    const SpindexConfig *config
) {
    if (!all_loaded_plugins || num_all_plugins <= 0 || !config) {
        LOG_ERROR("SearchOrchestrator create: Invalid arguments.");
        return NULL;
    }

    SearchOrchestrator* orchestrator = (SearchOrchestrator*)malloc(sizeof(SearchOrchestrator));
    if (!orchestrator) {
        LOG_ERROR("SearchOrchestrator create: Failed to allocate memory for orchestrator.");
        return NULL;
    }

    orchestrator->all_plugins_list = all_loaded_plugins;
    orchestrator->num_plugins = num_all_plugins;
    orchestrator->app_config = config;
    orchestrator->avx_search_plugin = NULL;
    orchestrator->es_client = NULL; // Placeholder

    // Find and cache the AVX search plugin
    for (int i = 0; i < num_all_plugins; ++i) {
        if (strcmp(all_loaded_plugins[i].info.name, "AVX-512 Phrase Searcher") == 0) {
            orchestrator->avx_search_plugin = &all_loaded_plugins[i];
            LOG_INFO("SearchOrchestrator: Found and cached AVX-512 Phrase Searcher plugin.");
            break;
        }
    }

    if (!orchestrator->avx_search_plugin) {
        LOG_ERROR("SearchOrchestrator: AVX-512 Phrase Searcher plugin NOT FOUND. AVX search pass will be disabled.");
        // Not returning NULL here, as the orchestrator might still function for ES-only searches if designed so.
    }

    // Placeholder for Elasticsearch client initialization
    // orchestrator->es_client = es_client_create(config->elasticsearch_url, ...);
    // if (!orchestrator->es_client) {
    //     LOG_ERROR("SearchOrchestrator: Failed to initialize Elasticsearch client.");
    //     free(orchestrator);
    //     return NULL;
    // }
    LOG_INFO("SearchOrchestrator: Elasticsearch client initialization placeholder.");

    LOG_INFO("SearchOrchestrator created successfully.");
    return orchestrator;
}

SearchResults* search_orchestrator_execute_query(
    SearchOrchestrator* orchestrator,
    const UserQuery* query
) {
    if (!orchestrator || !query || !query->main_query_text) {
        LOG_ERROR("Execute query: Invalid orchestrator or query.");
        return NULL;
    }

    LOG_INFO("Executing query: ES part = \"%s\", AVX phrases = \"%s\"",
             query->main_query_text, query->avx_phrases ? query->avx_phrases : "N/A");

    SearchResults* final_results = (SearchResults*)calloc(1, sizeof(SearchResults));
    if (!final_results) {
        LOG_ERROR("Execute query: Failed to allocate memory for SearchResults.");
        return NULL;
    }
    final_results->items = (SearchResultItem*)malloc(INITIAL_SEARCH_RESULTS_CAPACITY * sizeof(SearchResultItem));
    if (!final_results->items) {
        LOG_ERROR("Execute query: Failed to allocate memory for SearchResultItems.");
        free(final_results);
        return NULL;
    }
    final_results->capacity = INITIAL_SEARCH_RESULTS_CAPACITY;
    final_results->count = 0;

    // --- Pass 1: (Simulated) Elasticsearch Broad Search ---
    LOG_INFO("Pass 1: Simulating Elasticsearch search for query: '%s'", query->main_query_text);
    // For now, simulate ES results. In a real scenario, this would involve:
    // 1. Calling es_client_search(orchestrator->es_client, query->main_query_text, ...);
    // 2. Receiving a list of document IDs, scores, and possibly content snippets.

    const char* simulated_doc_ids[] = {"doc_001", "doc_002", "doc_003"};
    const char* simulated_doc_content[] = {
        "This is document one with some common text but no specific AVX target phrases.",
        "Document two contains the target phrase for AVX: 'special AVX keyword here'.",
        "The third document also has 'another AVX target' and some other information."
    };
    double simulated_scores[] = {0.9, 0.85, 0.8};
    int num_simulated_docs = sizeof(simulated_doc_ids) / sizeof(simulated_doc_ids[0]);

    // --- Pass 2: AVX-512 Precise Search (Loop through simulated ES results) ---
    if (orchestrator->avx_search_plugin && query->avx_phrases && strlen(query->avx_phrases) > 0) {
        LOG_INFO("Pass 2: Performing AVX search on ES results.");

        ProcessedDataChunk* phrases_chunk = create_text_chunk(query->avx_phrases, "avx_phrases_query", 0);
        if (!phrases_chunk) {
            LOG_ERROR("Failed to create phrases_chunk for AVX search.");
            // Continue without AVX pass, or handle error more gracefully
        } else {
            for (int i = 0; i < num_simulated_docs; ++i) {
                LOG_DEBUG("AVX Pass on doc_id: %s", simulated_doc_ids[i]);
                ProcessedDataChunk* text_to_search_chunk = create_text_chunk(simulated_doc_content[i], "es_result", i);
                if (!text_to_search_chunk) {
                    LOG_ERROR("Failed to create text_to_search_chunk for doc_id %s", simulated_doc_ids[i]);
                    continue;
                }

                AvxSearchParams avx_params;
                avx_params.text_to_search = text_to_search_chunk;
                avx_params.phrases_to_find = phrases_chunk;

                ProcessedDataChunk wrapper_chunk_for_avx;
                wrapper_chunk_for_avx.data = (unsigned char*)&avx_params;
                wrapper_chunk_for_avx.length = sizeof(AvxSearchParams);
                // Using CHUNK_TYPE_RAW_DATA as a placeholder for CHUNK_TYPE_OPAQUE_PARAMS
                // Ensure data_types.h has a suitable type or add one.
                wrapper_chunk_for_avx.data_type = CHUNK_TYPE_RAW_DATA;
                char source_id_buf[64];
                snprintf(source_id_buf, sizeof(source_id_buf), "avx_req_for_%s", simulated_doc_ids[i]);
                wrapper_chunk_for_avx.source_id = strdup(source_id_buf);


                ProcessedDataChunk* avx_result_chunk = NULL;
                if (orchestrator->avx_search_plugin->process_data) {
                     avx_result_chunk = orchestrator->avx_search_plugin->process_data(&wrapper_chunk_for_avx);
                } else {
                    LOG_ERROR("AVX Search plugin's process_data function is NULL!");
                }


                // Prepare SearchResultItem (even if AVX not run or fails, to keep ES results)
                if (final_results->count >= final_results->capacity) {
                    // Basic resize logic (double capacity)
                    size_t new_capacity = final_results->capacity * 2;
                    SearchResultItem* new_items = (SearchResultItem*)realloc(final_results->items, new_capacity * sizeof(SearchResultItem));
                    if (!new_items) {
                        LOG_ERROR("Failed to realloc SearchResultItems. Some results may be lost.");
                        // Free AVX result chunk and wrapper source_id if allocated
                        if (avx_result_chunk) {
                            if (avx_result_chunk->data) free(avx_result_chunk->data);
                            if (avx_result_chunk->source_id) free(avx_result_chunk->source_id);
                            free(avx_result_chunk);
                        }
                        if (wrapper_chunk_for_avx.source_id) free(wrapper_chunk_for_avx.source_id);
                        if (text_to_search_chunk->data) free(text_to_search_chunk->data);
                        if (text_to_search_chunk->source_id) free(text_to_search_chunk->source_id);
                        free(text_to_search_chunk);
                        continue; // Skip adding this item
                    }
                    final_results->items = new_items;
                    final_results->capacity = new_capacity;
                }

                SearchResultItem* current_item = &final_results->items[final_results->count];
                current_item->document_id = strdup(simulated_doc_ids[i]);
                current_item->document_content_preview = strdup(simulated_doc_content[i]); // Full content for now
                current_item->score = simulated_scores[i];
                current_item->avx_match_found = false;
                current_item->avx_match_details = NULL;

                if (avx_result_chunk && avx_result_chunk->data) {
                    current_item->avx_match_details = strdup((char*)avx_result_chunk->data);
                    // Simple check for "Found:" to set avx_match_found
                    if (strstr((char*)avx_result_chunk->data, "Found:") != NULL) {
                        current_item->avx_match_found = true;
                        LOG_INFO("AVX matches found in doc_id: %s", simulated_doc_ids[i]);
                    }
                    if (avx_result_chunk->data) free(avx_result_chunk->data);
                    if (avx_result_chunk->source_id) free(avx_result_chunk->source_id);
                    free(avx_result_chunk);
                } else {
                     LOG_WARN("AVX search returned no result or empty data for doc_id: %s", simulated_doc_ids[i]);
                     current_item->avx_match_details = strdup("AVX search not performed or no results.");
                }
                final_results->count++;

                // Cleanup per-document AVX resources
                if (wrapper_chunk_for_avx.source_id) free(wrapper_chunk_for_avx.source_id);
                if (text_to_search_chunk->data) free(text_to_search_chunk->data);
                if (text_to_search_chunk->source_id) free(text_to_search_chunk->source_id);
                free(text_to_search_chunk);
            }
            // Cleanup phrases_chunk
            if (phrases_chunk->data) free(phrases_chunk->data);
            if (phrases_chunk->source_id) free(phrases_chunk->source_id);
            free(phrases_chunk);
        }
    } else {
        // No AVX plugin or no AVX phrases, just populate with ES results
        LOG_INFO("Skipping AVX search pass (plugin not available or no AVX phrases).");
        for (int i = 0; i < num_simulated_docs; ++i) {
             if (final_results->count >= final_results->capacity) { /*Resize logic - omitted for brevity but same as above*/ }
            SearchResultItem* current_item = &final_results->items[final_results->count];
            current_item->document_id = strdup(simulated_doc_ids[i]);
            current_item->document_content_preview = strdup(simulated_doc_content[i]);
            current_item->score = simulated_scores[i];
            current_item->avx_match_found = false;
            current_item->avx_match_details = strdup("AVX search not performed.");
            final_results->count++;
        }
    }

    LOG_INFO("Search execution finished. Returning %zu results.", final_results->count);
    return final_results;
}

void search_results_destroy(SearchResults* results) {
    if (!results) return;
    if (results->items) {
        for (size_t i = 0; i < results->count; ++i) {
            if (results->items[i].document_id) free(results->items[i].document_id);
            if (results->items[i].document_content_preview) free(results->items[i].document_content_preview);
            if (results->items[i].avx_match_details) free(results->items[i].avx_match_details);
        }
        free(results->items);
    }
    free(results);
    LOG_DEBUG("SearchResults destroyed.");
}

void search_orchestrator_destroy(SearchOrchestrator* orchestrator) {
    if (!orchestrator) return;

    // Placeholder for Elasticsearch client destruction
    // if (orchestrator->es_client) {
    //     es_client_destroy(orchestrator->es_client);
    // }
    LOG_INFO("SearchOrchestrator: Elasticsearch client destruction placeholder.");

    // Note: Orchestrator does not own all_plugins_list or app_config, so it doesn't free them.
    // It also doesn't own the individual plugins within the list.

    free(orchestrator);
    LOG_INFO("SearchOrchestrator destroyed.");
}
