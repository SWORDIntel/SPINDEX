#define _GNU_SOURCE // For strdup
#include "../include/elasticsearch_client.h"
#include "../include/utils.h" // For logging (LOG_INFO, LOG_ERROR, etc.)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct to hold response data from curl
typedef struct {
    char *memory;
    size_t size;
} CurlResponse;

// Callback function for libcurl to write received data
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    CurlResponse *mem = (CurlResponse *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        LOG_ERROR("es_client: not enough memory (realloc returned NULL) in write_memory_callback");
        return 0; // This will cause libcurl to abort the transfer
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0; // Null-terminate

    return realsize;
}

ElasticsearchClient* es_client_init(const char* base_url) {
    if (!base_url) {
        LOG_ERROR("es_client_init: base_url is NULL.");
        return NULL;
    }

    // Initialize libcurl globally (safe to call multiple times)
    // CURL_GLOBAL_DEFAULT is usually sufficient.
    CURLcode global_init_res = curl_global_init(CURL_GLOBAL_SSL); // SSL for https if needed
    if (global_init_res != CURLE_OK) {
        LOG_ERROR("es_client_init: curl_global_init() failed: %s", curl_easy_strerror(global_init_res));
        return NULL;
    }

    ElasticsearchClient* client = (ElasticsearchClient*)malloc(sizeof(ElasticsearchClient));
    if (!client) {
        LOG_ERROR("es_client_init: Failed to allocate memory for ElasticsearchClient.");
        curl_global_cleanup(); // Clean up global curl state if client alloc fails
        return NULL;
    }

    client->base_url = strdup(base_url);
    if (!client->base_url) {
        LOG_ERROR("es_client_init: Failed to duplicate base_url string.");
        free(client);
        curl_global_cleanup();
        return NULL;
    }

    client->curl_handle = curl_easy_init();
    if (!client->curl_handle) {
        LOG_ERROR("es_client_init: curl_easy_init() failed.");
        free(client->base_url);
        free(client);
        curl_global_cleanup();
        return NULL;
    }

    client->default_headers = NULL;
    client->default_headers = curl_slist_append(client->default_headers, "Content-Type: application/json");
    if(!client->default_headers){
        LOG_ERROR("es_client_init: curl_slist_append for Content-Type failed.");
        curl_easy_cleanup(client->curl_handle);
        free(client->base_url);
        free(client);
        curl_global_cleanup();
        return NULL;
    }

    LOG_INFO("Elasticsearch client initialized with base URL: %s", base_url);
    return client;
}

void es_client_destroy(ElasticsearchClient* client) {
    if (client) {
        if (client->curl_handle) {
            curl_easy_cleanup(client->curl_handle);
        }
        if (client->default_headers) {
            curl_slist_free_all(client->default_headers);
        }
        if (client->base_url) {
            free(client->base_url);
        }
        free(client);
        LOG_INFO("Elasticsearch client destroyed.");
    }
    // Clean up libcurl globally
    curl_global_cleanup();
}

int es_index_document(ElasticsearchClient* client, const char* index_name, const char* doc_id, const char* document_json) {
    if (!client || !client->curl_handle || !index_name || !document_json) {
        LOG_ERROR("es_index_document: Invalid arguments.");
        return -1;
    }

    CURL* curl = client->curl_handle;
    CURLcode res;
    char url_buffer[1024];
    long http_code = 0;

    // Construct URL
    if (doc_id && strlen(doc_id) > 0) {
        snprintf(url_buffer, sizeof(url_buffer), "%s/%s/_doc/%s", client->base_url, index_name, doc_id);
    } else {
        // Let Elasticsearch generate the ID
        snprintf(url_buffer, sizeof(url_buffer), "%s/%s/_doc", client->base_url, index_name);
    }

    CurlResponse response_chunk;
    response_chunk.memory = (char*)malloc(1); // Will be grown by realloc
    response_chunk.size = 0;
    if(response_chunk.memory == NULL){
        LOG_ERROR("es_index_document: Failed to allocate memory for response buffer.");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url_buffer);
    // For PUT (if doc_id is specified) or POST (if doc_id is NULL for auto-generation)
    if (doc_id && strlen(doc_id) > 0) {
         curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); // Or CURLOPT_PUT, but CUSTOMREQUEST is more flexible
    } else {
         curl_easy_setopt(curl, CURLOPT_POST, 1L);
    }
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, document_json);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(document_json));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, client->default_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response_chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L); // 10 seconds timeout

    LOG_DEBUG("Indexing to URL: %s, Data: %s", url_buffer, document_json);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("es_index_document: curl_easy_perform() failed: %s", curl_easy_strerror(res));
        free(response_chunk.memory);
        return -1;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG_INFO("es_index_document: Response code: %ld", http_code);
    LOG_DEBUG("es_index_document: Response body: %s", response_chunk.memory ? response_chunk.memory : "N/A");

    free(response_chunk.memory);

    if (http_code == 200 || http_code == 201) { // 200 OK for update, 201 Created for new
        return 0; // Success
    }

    return -1; // Failure
}

int es_check_health(ElasticsearchClient* client) {
    if (!client || !client->curl_handle) {
        LOG_ERROR("es_check_health: Invalid client or curl_handle.");
        return -1;
    }

    CURL* curl = client->curl_handle;
    CURLcode res;
    char url_buffer[512];
    long http_code = 0;

    snprintf(url_buffer, sizeof(url_buffer), "%s/_cluster/health", client->base_url);

    CurlResponse response_chunk;
    response_chunk.memory = (char*)malloc(1);
    response_chunk.size = 0;
     if(response_chunk.memory == NULL){
        LOG_ERROR("es_check_health: Failed to allocate memory for response buffer.");
        return -1;
    }

    // Reset options that might have been set by other functions
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_POST, 0L); // Ensure it's not a POST
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL); // Reset custom request
    // For GET, Content-Type header is usually not needed, but curl_slist_append is additive.
    // If we had other headers specific to PUT/POST, we would need to manage them.
    // For this simple client, default_headers only has Content-Type which is fine for ES to ignore on GET.

    curl_easy_setopt(curl, CURLOPT_URL, url_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response_chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L); // 5 seconds timeout

    LOG_DEBUG("Checking ES health at URL: %s", url_buffer);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("es_check_health: curl_easy_perform() failed: %s", curl_easy_strerror(res));
        free(response_chunk.memory);
        return -1;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG_INFO("es_check_health: Response code: %ld", http_code);
    LOG_DEBUG("es_check_health: Response body: %s", response_chunk.memory ? response_chunk.memory : "N/A");


    if (http_code == 200) {
        // Basic check: a 200 OK means the cluster is reachable and responding.
        // A more detailed check would parse the JSON response for "status": "green" or "yellow".
        // For now, 200 OK is "healthy enough" for this basic client.
        // Example: if (strstr(response_chunk.memory, "\"status\":\"green\"") || strstr(response_chunk.memory, "\"status\":\"yellow\""))
        LOG_INFO("Elasticsearch cluster health check OK (HTTP %ld).", http_code);
        free(response_chunk.memory);
        return 0; // Success
    }

    LOG_WARN("Elasticsearch cluster health check failed or status not green/yellow (HTTP %ld).", http_code);
    free(response_chunk.memory);
    return -1; // Failure or non-ideal status
}
