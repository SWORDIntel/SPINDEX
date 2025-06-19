#define _GNU_SOURCE // For strdup
#include "../include/elasticsearch_client.h"
#include "../include/utils.h" // For logging (LOG_INFO, LOG_ERROR, etc.)
#include "../include/plugin_interface.h" // For SpindexPlugin, already included via elasticsearch_client.h but good for clarity
#include "../include/data_types.h"       // For ProcessedDataChunk and CHUNK_TYPE_*
#include "../include/avx_search_params.h"  // For AvxSearchParams, already included via elasticsearch_client.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// No jansson.h here, as per instructions

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

char* es_search_documents(ElasticsearchClient* client, const char* index_name, const char* query_json) {
    if (!client || !client->curl_handle || !index_name || !query_json) {
        LOG_ERROR("es_search_documents: Invalid arguments.");
        return NULL;
    }

    CURL* curl = client->curl_handle;
    CURLcode res;
    char url_buffer[1024];
    long http_code = 0;

    // Construct URL: <base_url>/<index_name>/_search
    snprintf(url_buffer, sizeof(url_buffer), "%s/%s/_search", client->base_url, index_name);

    CurlResponse response_chunk;
    response_chunk.memory = (char*)malloc(1); // Will be grown by realloc
    response_chunk.size = 0;
    if (response_chunk.memory == NULL) {
        LOG_ERROR("es_search_documents: Failed to allocate memory for response buffer.");
        return NULL;
    }

    // Set cURL options for GET request with a body
    // Reset any custom request that might have been set (e.g. PUT from es_index_document)
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_URL, url_buffer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query_json);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(query_json));

    // Ensure default headers (like Content-Type: application/json) are set
    // This is important as Elasticsearch expects this for GET with body
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, client->default_headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response_chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L); // 15 seconds timeout for search operations

    LOG_DEBUG("Searching at URL: %s, Query: %s", url_buffer, query_json);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("es_search_documents: curl_easy_perform() failed: %s", curl_easy_strerror(res));
        free(response_chunk.memory);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG_INFO("es_search_documents: Response code: %ld", http_code);
    LOG_DEBUG("es_search_documents: Response body: %s", response_chunk.memory ? response_chunk.memory : "N/A");

    // Reset custom request method to NULL after use, so subsequent calls are not affected
    // if they don't explicitly set it (good practice).
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    // Also reset POSTFIELDS which is not cleared automatically by a subsequent GET without body
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);


    if (http_code == 200) { // HTTP 200 OK for successful search
        char* return_payload = response_chunk.memory;
        // Detach the memory from the struct so it's not freed if response_chunk is a local var that goes out of scope
        // The caller is now responsible for this memory.
        response_chunk.memory = NULL;
        response_chunk.size = 0;
        return return_payload;
    } else {
        LOG_ERROR("es_search_documents: Search failed with HTTP code %ld. Response: %s", http_code, response_chunk.memory ? response_chunk.memory : "N/A");
        free(response_chunk.memory);
        return NULL;
    }
}

// Helper function to find a plugin by its registered name
static SpindexPlugin* find_plugin_by_name(SpindexPlugin* plugins, int count, const char* name) {
    if (!plugins || !name) return NULL;
    for (int i = 0; i < count; ++i) {
        // Check if plugin info and name are not NULL to prevent crashes
        if (plugins[i].info.name && strcmp(plugins[i].info.name, name) == 0) {
            return &plugins[i];
        }
    }
    LOG_WARN("find_plugin_by_name: Plugin with name '%s' not found.", name);
    return NULL;
}

int es_search_and_refine_with_avx(
    ElasticsearchClient* client,
    SpindexPlugin* loaded_plugins,
    int plugin_count,
    const char* index_name,
    const char* es_query_json,
    const char* phrases_for_avx_str,
    const char* avx_plugin_name,
    bool case_sensitive_avx,
    bool find_all_occurrences_avx,
    int max_hits_per_doc_avx,
    RefinedSearchResult** refined_results_array,
    int* refined_results_count
) {
    if (!client || !loaded_plugins || plugin_count <= 0 || !index_name || !es_query_json ||
        !phrases_for_avx_str || !avx_plugin_name || !refined_results_array || !refined_results_count) {
        LOG_ERROR("es_search_and_refine_with_avx: Invalid arguments provided.");
        if (refined_results_array) *refined_results_array = NULL;
        if (refined_results_count) *refined_results_count = 0;
        return -1;
    }

    *refined_results_array = NULL;
    *refined_results_count = 0;

    // 1. Find the AVX plugin
    SpindexPlugin* avx_plugin = find_plugin_by_name(loaded_plugins, plugin_count, avx_plugin_name);
    if (!avx_plugin) {
        LOG_ERROR("es_search_and_refine_with_avx: AVX plugin '%s' not found.", avx_plugin_name);
        return -1;
    }
    if (!avx_plugin->process_data) {
        LOG_ERROR("es_search_and_refine_with_avx: AVX plugin '%s' does not have a process_data function.", avx_plugin_name);
        return -1;
    }
    LOG_INFO("es_search_and_refine_with_avx: Found AVX plugin: %s", avx_plugin->info.name);

    // 2. Call Elasticsearch to get initial results
    char* es_response_json = es_search_documents(client, index_name, es_query_json);
    if (!es_response_json) {
        LOG_ERROR("es_search_and_refine_with_avx: Elasticsearch search failed for index '%s'.", index_name);
        return -1;
    }
    LOG_DEBUG("es_search_and_refine_with_avx: Elasticsearch returned: %s", es_response_json);

    // --- Placeholder for ES Response Parsing and Iterating Hits ---
    // In a real implementation, you would parse es_response_json (e.g., using Jansson)
    // and iterate through each hit. For each hit, you'd extract the document ID
    // and the relevant text field(s) to pass to the AVX plugin.

    // For this subtask, simulate extracting one document's text and ID:
    const char* placeholder_doc_id_from_es = "doc1_from_es";
    const char* placeholder_doc_text_from_es = "This is some sample text from Elasticsearch for doc1 that might contain the target phrase or phrases.";
    LOG_INFO("es_search_and_refine_with_avx: Using placeholder document ID '%s' and text for AVX processing.", placeholder_doc_id_from_es);

    // 3. Prepare Data for AVX Plugin (for the placeholder document)
    ProcessedDataChunk text_chunk;
    text_chunk.data_type = CHUNK_TYPE_UTF8_TEXT;
    text_chunk.data = (void*)placeholder_doc_text_from_es; // No strdup needed as it's a const string literal for placeholder
    text_chunk.length = strlen(placeholder_doc_text_from_es);
    text_chunk.source_id = strdup(placeholder_doc_id_from_es); // Needs to be freed
    if (!text_chunk.source_id) {
        LOG_ERROR("es_search_and_refine_with_avx: Failed to strdup source_id for text_chunk.");
        free(es_response_json);
        return -1;
    }


    ProcessedDataChunk phrases_chunk;
    phrases_chunk.data_type = CHUNK_TYPE_UTF8_TEXT; // Assuming phrases are also UTF-8 text
    phrases_chunk.data = (void*)phrases_for_avx_str;  // No strdup, comes from arg
    phrases_chunk.length = strlen(phrases_for_avx_str);
    phrases_chunk.source_id = NULL; // Not strictly needed for phrases if not per-doc

    AvxSearchParams avx_params;
    avx_params.text_to_search_in = &text_chunk;
    avx_params.phrases_to_find = &phrases_chunk;
    avx_params.case_sensitive = case_sensitive_avx;
    avx_params.find_all_occurrences = find_all_occurrences_avx;
    avx_params.max_hits_per_doc = max_hits_per_doc_avx;
    // avx_params.context_window_size = 0; // Default or set if needed

    ProcessedDataChunk wrapper_chunk;
    wrapper_chunk.data_type = CHUNK_TYPE_AVX_SEARCH_PARAMS; // Using the specific type from avx_search_params.h
    wrapper_chunk.data = &avx_params;
    wrapper_chunk.length = sizeof(AvxSearchParams);
    wrapper_chunk.source_id = strdup(placeholder_doc_id_from_es); // Needs to be freed
     if (!wrapper_chunk.source_id) {
        LOG_ERROR("es_search_and_refine_with_avx: Failed to strdup source_id for wrapper_chunk.");
        free(text_chunk.source_id);
        free(es_response_json);
        return -1;
    }


    // 4. Call AVX Plugin
    LOG_INFO("es_search_and_refine_with_avx: Calling AVX plugin '%s' for doc '%s'", avx_plugin_name, placeholder_doc_id_from_es);
    ProcessedDataChunk* avx_result_chunk = avx_plugin->process_data(&wrapper_chunk);

    // 5. Process AVX Result (Placeholder for one doc)
    if (avx_result_chunk && avx_result_chunk->data && avx_result_chunk->data_type == CHUNK_TYPE_JSON_TEXT) {
        LOG_INFO("AVX plugin result for doc '%s': %s", placeholder_doc_id_from_es, (char*)avx_result_chunk->data);

        // Simulate adding one result to the output array
        *refined_results_array = (RefinedSearchResult*)malloc(sizeof(RefinedSearchResult));
        if (!(*refined_results_array)) {
            LOG_ERROR("es_search_and_refine_with_avx: Failed to allocate memory for refined_results_array.");
            // Cleanup AVX result
            if (avx_result_chunk->source_id) free(avx_result_chunk->source_id);
            if (avx_result_chunk->data) free(avx_result_chunk->data);
            free(avx_result_chunk);
            // Cleanup other things
            free(wrapper_chunk.source_id);
            free(text_chunk.source_id);
            free(es_response_json);
            return -1;
        }

        (*refined_results_array)[0].doc_id = strdup(placeholder_doc_id_from_es);
        (*refined_results_array)[0].doc_content_excerpt = strdup(placeholder_doc_text_from_es); // Placeholder
        (*refined_results_array)[0].avx_match_details_json = strdup((char*)avx_result_chunk->data);

        // Basic check for "offsets": [" or "offsets": [[ to determine if matches were found.
        // A more robust check would parse the JSON.
        const char* avx_json_data = (const char*)avx_result_chunk->data;
        bool match_found_flag = false;
        if (strstr(avx_json_data, "\"offsets\": [") && !strstr(avx_json_data, "\"offsets\": []")) {
             match_found_flag = true; // Found non-empty array: "offsets": [ ... ]
        } else if (strstr(avx_json_data, "\"offsets\": [{")) {
             match_found_flag = true; // Found array with objects: "offsets": [{...}]
        }


        (*refined_results_array)[0].avx_match_found = match_found_flag;

        if (!(*refined_results_array)[0].doc_id || !(*refined_results_array)[0].doc_content_excerpt || !(*refined_results_array)[0].avx_match_details_json) {
            LOG_ERROR("es_search_and_refine_with_avx: Failed to strdup data for RefinedSearchResult.");
            // Free any successfully strdup'd fields before freeing the struct itself
            if ((*refined_results_array)[0].doc_id) free((*refined_results_array)[0].doc_id);
            if ((*refined_results_array)[0].doc_content_excerpt) free((*refined_results_array)[0].doc_content_excerpt);
            if ((*refined_results_array)[0].avx_match_details_json) free((*refined_results_array)[0].avx_match_details_json);
            free(*refined_results_array);
            *refined_results_array = NULL;
            // Cleanup AVX result
            if (avx_result_chunk->source_id) free(avx_result_chunk->source_id);
            if (avx_result_chunk->data) free(avx_result_chunk->data);
            free(avx_result_chunk);
            // Cleanup other things
            free(wrapper_chunk.source_id);
            free(text_chunk.source_id);
            free(es_response_json);
            return -1;
        }
        *refined_results_count = 1;
        LOG_INFO("es_search_and_refine_with_avx: Added 1 placeholder result to output array.");

    } else {
        LOG_WARN("AVX plugin for doc '%s' returned no data or unexpected data type (expected CHUNK_TYPE_JSON_TEXT). Type was: %d",
                 placeholder_doc_id_from_es, avx_result_chunk ? avx_result_chunk->data_type : -1);
        // Even if AVX fails for one doc, we might continue with others in a real loop.
        // For this placeholder, we'll just note it.
    }

    // 6. Cleanup
    if (avx_result_chunk) {
        // According to plugin_interface.h, the caller of process_data is responsible
        // for freeing the returned ProcessedDataChunk and its contents if non-NULL.
        // The plugin should strdup source_id and data if it allocates them.
        if (avx_result_chunk->source_id) free(avx_result_chunk->source_id);
        if (avx_result_chunk->data) free(avx_result_chunk->data);
        free(avx_result_chunk);
    }

    free(wrapper_chunk.source_id); // strdup'd for wrapper_chunk
    free(text_chunk.source_id);    // strdup'd for text_chunk
    free(es_response_json);        // From es_search_documents

    LOG_INFO("es_search_and_refine_with_avx: Placeholder processing complete.");
    return 0; // Success for placeholder
}
