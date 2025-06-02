#ifndef ELASTICSEARCH_CLIENT_H
#define ELASTICSEARCH_CLIENT_H

#include <curl/curl.h> // For CURL handle type

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

#endif // ELASTICSEARCH_CLIENT_H
