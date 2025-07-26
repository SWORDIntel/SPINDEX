#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../include/plugin_interface.h"
#include "../include/data_types.h"
#include "../include/plugin_loader.h"
#include "../plugins/indexers/elasticsearch_indexer.h"

void test_successful_integration() {
    printf("Running test: test_successful_integration\n");

    SpindexPlugin* indexer_plugin = spindex_load_plugin("plugins/indexers/elasticsearch_indexer.so");
    assert(indexer_plugin != NULL);

    const char* test_data = "{\"content\": \"hello world, this is a test\"}";
    ProcessedDataChunk input_chunk = {
        .data = (unsigned char*)test_data,
        .length = strlen(test_data),
        .data_type = CHUNK_TYPE_UTF8_TEXT
    };

    ProcessedDataChunk* output_chunk = indexer_plugin->process_data(&input_chunk);
    assert(output_chunk == NULL); // The indexer plugin does not return a chunk

    // This test is limited in that it cannot easily verify the content of the Elasticsearch index.
    // A more comprehensive test would require a running Elasticsearch instance and a client to inspect the index.

    spindex_unload_plugin(indexer_plugin);
    printf("Test passed\n");
}

void test_no_avx_plugin() {
    printf("Running test: test_no_avx_plugin\n");

    // To simulate the AVX plugin not being available, we can rename it temporarily.
    int ret = rename("plugins/search_algorithms/avx512_phrase_search.so", "plugins/search_algorithms/avx512_phrase_search.so.bak");
    assert(ret == 0);

    SpindexPlugin* indexer_plugin = spindex_load_plugin("plugins/indexers/elasticsearch_indexer.so");
    assert(indexer_plugin != NULL);

    // In this case, the indexer should still work, but without phrase extraction.
    const char* test_data = "{\"content\": \"hello world, this is a test\"}";
    ProcessedDataChunk input_chunk = {
        .data = (unsigned char*)test_data,
        .length = strlen(test_data),
        .data_type = CHUNK_TYPE_UTF8_TEXT
    };

    ProcessedDataChunk* output_chunk = indexer_plugin->process_data(&input_chunk);
    assert(output_chunk == NULL);

    spindex_unload_plugin(indexer_plugin);

    ret = rename("plugins/search_algorithms/avx512_phrase_search.so.bak", "plugins/search_algorithms/avx512_phrase_search.so");
    assert(ret == 0);

    printf("Test passed\n");
}

int main() {
    test_successful_integration();
    test_no_avx_plugin();
    return 0;
}
