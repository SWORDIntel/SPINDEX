#include "../include/elasticsearch_client.h"
#include "../include/utils.h" // For LOG_INFO, LOG_ERROR (indirectly via es_client.c)
#include <stdio.h> // For printf

int main() {
    // Initialize logger for the test itself (optional, as client uses it)
    // spindex_log_init(NULL, LOG_LEVEL_DEBUG); // Log to stdout with DEBUG level

    printf("--- Starting Elasticsearch Client Test ---\n");

    ElasticsearchClient* client = es_client_init("http://localhost:9200");
    if (!client) {
        printf("Failed to initialize Elasticsearch client. Aborting test.\n");
        // spindex_log_shutdown();
        return 1;
    }
    printf("Elasticsearch client initialized.\n");

    printf("\n--- Checking Elasticsearch Cluster Health ---\n");
    if (es_check_health(client) == 0) {
        printf("Elasticsearch cluster health: OK\n");
    } else {
        printf("Elasticsearch cluster health: FAILED or NOT GREEN/YELLOW. Check logs.\n");
        // For this test, we'll continue even if health check fails,
        // to see if indexing works (ES might be up but yellow).
    }

    printf("\n--- Indexing Sample Document --- \n");
    const char* index_name = "test_spindex_idx";
    const char* doc_id_1 = "doc1";
    const char* doc_json_1 = "{\"message\":\"Hello from SPINDEX client! Document 1\", \"source\":\"test_harness\"}";

    printf("Indexing document with ID '%s' to index '%s'...\n", doc_id_1, index_name);
    if (es_index_document(client, index_name, doc_id_1, doc_json_1) == 0) {
        printf("Document '%s' indexed successfully.\n", doc_id_1);
    } else {
        printf("Failed to index document '%s'. Check logs.\n", doc_id_1);
    }

    printf("\n--- Indexing Another Sample Document (auto-ID) --- \n");
    const char* doc_json_2 = "{\"message\":\"Hello again! Document 2 with auto-generated ID\", \"user\":\"spindex_tester\"}";
    printf("Indexing document (auto-ID) to index '%s'...\n", index_name);
    if (es_index_document(client, index_name, NULL, doc_json_2) == 0) {
        printf("Document (auto-ID) indexed successfully.\n");
    } else {
        printf("Failed to index document (auto-ID). Check logs.\n");
    }

    printf("\nDestroying Elasticsearch client...\n");
    es_client_destroy(client);
    printf("Elasticsearch client destroyed.\n");

    // spindex_log_shutdown(); // If logger was initialized by test
    printf("\n--- Elasticsearch Client Test Complete ---\n");
    printf("Note: This test relies on an Elasticsearch instance running at http://localhost:9200.\n");
    printf("Check application logs (if configured) and Elasticsearch logs for details.\n");

    return 0;
}
