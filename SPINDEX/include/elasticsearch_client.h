#ifndef ELASTICSEARCH_CLIENT_H
#define ELASTICSEARCH_CLIENT_H

#include <curl/curl.h> // For CURL handle type
#include <stdbool.h>   // For bool type in RefinedSearchResult

#include "../include/plugin_interface.h" // For SpindexPlugin
#include "../include/avx_search_params.h"  // For AvxSearchParams (though might be indirect)


// Opaque struct for ES client instance
typedef struct {
    char* base_url;
    CURL* curl_handle; // Reusable CURL easy handle
    struct curl_slist* default_headers; // For Content-Type: application/json
} ElasticsearchClient;

/**
 * @brief Initializes the Elasticsearch client.
 *
 * This function must be called before any other es_client_* functions.
 * It initializes libcurl and prepares a reusable CURL handle.
 *
 * @param base_url The base URL of the Elasticsearch cluster (e.g., "http://localhost:9200").
 * @return A pointer to an ElasticsearchClient instance, or NULL on failure.
 *         The caller is responsible for freeing this with es_client_destroy.
 */
ElasticsearchClient* es_client_init(const char* base_url);

/**
 * @brief Destroys the Elasticsearch client and cleans up resources.
 *
 * @param client Pointer to the ElasticsearchClient instance to destroy.
 */
void es_client_destroy(ElasticsearchClient* client);

/**
 * @brief Indexes a document in Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param index_name The name of the index.
 * @param doc_id The ID of the document. If NULL, Elasticsearch will generate one.
 * @param document_json The JSON string representing the document.
 * @return 0 on success (HTTP 200 or 201), -1 on failure.
 */
int es_index_document(ElasticsearchClient* client, const char* index_name, const char* doc_id, const char* document_json);

/**
 * @brief Checks the health of the Elasticsearch cluster.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @return 0 if the cluster is considered healthy (e.g., green or yellow status),
 *         -1 otherwise (e.g., red status or request failure).
 */
int es_check_health(ElasticsearchClient* client);

/**
 * @brief Retrieves a document from Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param index_name The name of the index.
 * @param doc_id The ID of the document.
 * @return A string containing the document JSON if found (HTTP 200), or NULL on failure or if not found.
 *         The caller is responsible for freeing the returned string.
 */
char* es_get_document(ElasticsearchClient* client, const char* index_name, const char* doc_id);

/**
 * @brief Deletes a document from Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param index_name The name of the index.
 * @param doc_id The ID of the document.
 * @return 0 on success (HTTP 200), -1 on failure (e.g. not found HTTP 404, or other errors).
 */
int es_delete_document(ElasticsearchClient* client, const char* index_name, const char* doc_id);

/**
 * @brief Checks if an index exists in Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param index_name The name of the index.
 * @return 0 if the index exists (HTTP 200), 1 if it does not exist (HTTP 404), -1 on other errors.
 */
int es_index_exists(ElasticsearchClient* client, const char* index_name);

/**
 * @brief Deletes an index from Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param index_name The name of the index to delete.
 * @return 0 on success (HTTP 200), -1 on failure.
 */
int es_delete_index(ElasticsearchClient* client, const char* index_name);

/**
 * @brief Performs bulk operations in Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param bulk_json_payload A string containing the bulk operations in Elasticsearch NDJSON format.
 * @return A string containing the response JSON from Elasticsearch. The caller is responsible for freeing this.
 *         Returns NULL on request failure or if the response body is empty.
 */
char* es_bulk_operations(ElasticsearchClient* client, const char* bulk_json_payload);

/**
 * @brief Searches for documents in Elasticsearch.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param index_name The name of the index to search in.
 * @param query_json The JSON string representing the search query.
 * @return A string containing the search results JSON from Elasticsearch (HTTP 200).
 *         The caller is responsible for freeing the returned string.
 *         Returns NULL on any error or if the HTTP response code is not 200.
 */
char* es_search_documents(ElasticsearchClient* client, const char* index_name, const char* query_json);

/**
 * @brief Structure to hold refined search results after AVX processing.
 */
typedef struct {
    char* doc_id;                   // ID of the document from Elasticsearch
    char* doc_content_excerpt;      // Example: could be the field that was searched by AVX, or a relevant snippet
    bool avx_match_found;           // True if the AVX plugin found matches for the given phrases
    char* avx_match_details_json;   // Full JSON response from the AVX plugin for this document
    // Add other relevant fields like original score from ES if needed later
    // double es_score;
} RefinedSearchResult;

/**
 * @brief Performs a search on Elasticsearch, then refines results using an AVX plugin.
 *
 * This function first queries Elasticsearch, then for each hit (or a subset),
 * it invokes the specified AVX plugin to perform a more precise search (e.g., phrase search)
 * within the document content.
 *
 * @param client Pointer to the ElasticsearchClient instance.
 * @param loaded_plugins Array of loaded Spindex plugins.
 * @param plugin_count Number of plugins in the loaded_plugins array.
 * @param index_name The Elasticsearch index to search.
 * @param es_query_json The query for Elasticsearch.
 * @param phrases_for_avx_str A string containing newline-separated phrases for the AVX plugin.
 * @param avx_plugin_name The registered name of the AVX search plugin to use.
 * @param case_sensitive_avx Whether the AVX search should be case-sensitive.
 * @param find_all_occurrences_avx Whether the AVX search should find all occurrences or just the first.
 * @param max_hits_per_doc_avx Maximum number of hits per document for the AVX search.
 * @param refined_results_array Output parameter: Pointer to an array of RefinedSearchResult structs.
 *                              The caller is responsible for freeing this array and its contents
 *                              (e.g., using a helper function like free_refined_results).
 * @param refined_results_count Output parameter: Pointer to an integer that will be filled with
 *                              the number of elements in refined_results_array.
 * @return 0 on success (even if no results are found but the process completes),
 *         -1 on critical errors (e.g., plugin not found, ES query failed, memory allocation issues).
 *         -2 if AVX plugin processing fails for all documents. (Specific error codes can be expanded)
 */
int es_search_and_refine_with_avx(
    ElasticsearchClient* client,
    SpindexPlugin* loaded_plugins, // From plugin_loader.h (via plugin_interface.h)
    int plugin_count,
    const char* index_name,
    const char* es_query_json,
    const char* phrases_for_avx_str, // Newline-separated phrases
    const char* avx_plugin_name,
    bool case_sensitive_avx,
    bool find_all_occurrences_avx,
    int max_hits_per_doc_avx,
    RefinedSearchResult** refined_results_array, // Output array
    int* refined_results_count                 // Output count
);

#endif // ELASTICSEARCH_CLIENT_H
