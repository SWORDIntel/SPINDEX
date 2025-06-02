#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/config_manager.h"
#include "../include/ingestion_pipeline.h" // Also brings in plugin_interface.h, data_types.h
#include "../include/plugin_loader.h"      // For unload_plugins (though pipeline_shutdown should handle its plugins)
#include "../include/elasticsearch_client.h" // For direct ES health check
#include "../include/utils.h"

int main() {
    spindex_log_init("test_elasticsearch_indexing.log", LOG_LEVEL_DEBUG);
    LOG_INFO("--- Starting Elasticsearch End-to-End Indexing Test ---");

    // 1. Load Configuration
    SpindexConfig config;
    memset(&config, 0, sizeof(SpindexConfig)); // Initialize to NULL/zero

    // Attempt to load from file, but use defaults if file not found.
    if (parse_config_file("test_spindex_es.conf", &config) != 0) {
        LOG_WARN("Could not load 'test_spindex_es.conf'. Using default settings.");
        set_default_config(&config); // Ensure defaults are set if file fails
        // Override plugin directory for test even if default config is used
        snprintf(config.plugin_directory, MAX_CONFIG_STR_LEN, "/app/SPINDEX/build/plugins_for_test");
        // Ensure essential ES settings and active indexer are defaults from set_default_config
        if (config.elasticsearch_url) free(config.elasticsearch_url);
        config.elasticsearch_url = strdup("http://localhost:9200");
        if (config.elasticsearch_index_name) free(config.elasticsearch_index_name);
        config.elasticsearch_index_name = strdup("spindex_e2e_default_idx");
        if (config.active_indexer_name) free(config.active_indexer_name);
        config.active_indexer_name = strdup("ElasticsearchIndexer");

    } else {
        // Config file loaded, but ensure plugin_directory is set for test environment
        LOG_INFO("Successfully loaded 'test_spindex_es.conf'.");
        snprintf(config.plugin_directory, MAX_CONFIG_STR_LEN, "/app/SPINDEX/build/plugins_for_test");
        LOG_INFO("Overriding plugin_directory for test: %s", config.plugin_directory);
    }

    LOG_INFO("Final configuration for test:");
    print_config(&config);


    // 2. Direct ES Health Check (Optional but Recommended)
    LOG_INFO("Performing direct Elasticsearch health check...");
    ElasticsearchClient* es_direct_client = es_client_init(config.elasticsearch_url);
    if (!es_direct_client) {
        LOG_ERROR("Failed to initialize direct ES client for health check. Test may be unreliable.");
    } else {
        if (es_check_health(es_direct_client) != 0) {
            LOG_WARN("Direct Elasticsearch health check FAILED. ES instance might not be running at '%s' or is unhealthy. Indexing will likely fail.", config.elasticsearch_url);
        } else {
            LOG_INFO("Direct Elasticsearch health check SUCCEEDED. Instance at '%s' is responsive.", config.elasticsearch_url);
        }
        es_client_destroy(es_direct_client);
        LOG_INFO("Direct ES client for health check destroyed.");
    }

    // 3. Load All Plugins (needed for pipeline init)
    SpindexPlugin *loaded_plugins_array = NULL;
    int num_loaded_plugins = 0;
    LOG_INFO("Loading plugins from: %s", config.plugin_directory);
    if (load_plugins(config.plugin_directory, &loaded_plugins_array, &num_loaded_plugins) != 0) {
        LOG_ERROR("Critical failure in load_plugins. Aborting test.");
        free_config_resources(&config);
        spindex_log_shutdown();
        return 1;
    }
    if (num_loaded_plugins == 0) {
        LOG_ERROR("No plugins were loaded from %s. Aborting test.", config.plugin_directory);
        // load_plugins frees the array if 0 plugins are loaded.
        free_config_resources(&config);
        spindex_log_shutdown();
        return 1;
    }
    LOG_INFO("Successfully loaded %d plugins.", num_loaded_plugins);


    // 4. Initialize Ingestion Pipeline
    SpindexIngestionPipeline pipeline;
    LOG_INFO("Initializing ingestion pipeline...");
    if (pipeline_init(&pipeline, &config, loaded_plugins_array, num_loaded_plugins) != 0) {
        LOG_ERROR("Failed to initialize ingestion pipeline. Aborting.");
        unload_plugins(loaded_plugins_array, num_loaded_plugins);
        free_config_resources(&config);
        spindex_log_shutdown();
        return 1;
    }
    LOG_INFO("Ingestion pipeline initialized successfully.");
    if(pipeline.active_indexer_plugin) {
        LOG_INFO("Active indexer in pipeline: %s", pipeline.active_indexer_plugin->info.name);
        LOG_WARN("Note: ElasticsearchIndexer plugin currently uses hardcoded ES URL/index from its own init due to plugin_loader limitations.");
        LOG_WARN("It will NOT use values from test_spindex_es.conf for actual indexing unless its code is changed or plugin_loader is enhanced.");
    } else {
        LOG_ERROR("No active indexer configured in pipeline despite 'active_indexer_name' in config. Check plugin loading and types.");
        // Continue for now, indexing step will be skipped by pipeline.
    }


    // 5. Process the Sample JSON File
    // const char* test_file_path = "../test_data/sample_es_test.json"; // Unused variable
    const char* abs_test_file_path = "/app/SPINDEX/test_data/sample_es_test.json";
    LOG_INFO("Attempting to process file for E2E indexing: %s", abs_test_file_path);

    ProcessedDataChunk* final_chunk = pipeline_process_file(&pipeline, abs_test_file_path);

    if (final_chunk) {
        LOG_WARN("Pipeline processed file but returned a data chunk. Expected NULL if indexer consumed it.");
        LOG_INFO("Returned chunk type: %d, length: %zu, source_id: %s",
                 final_chunk->data_type, final_chunk->length,
                 final_chunk->source_id ? final_chunk->source_id : "N/A");
        free_processed_data_chunk(final_chunk);
    } else {
        if (pipeline.active_indexer_plugin) {
            LOG_INFO("Pipeline processed file and returned NULL. Data assumed consumed by indexer '%s'.", pipeline.active_indexer_plugin->info.name);
        } else {
            LOG_WARN("Pipeline returned NULL, but no indexer was active. This might indicate an earlier processing error.");
        }
    }

    // 6. Shutdown Pipeline
    LOG_INFO("Shutting down ingestion pipeline...");
    pipeline_shutdown(&pipeline);
    LOG_INFO("Ingestion pipeline shut down.");

    // 7. Unload plugins
    LOG_INFO("Unloading plugins...");
    unload_plugins(loaded_plugins_array, num_loaded_plugins);
    LOG_INFO("Plugins unloaded.");

    // 8. Free Configuration Resources
    LOG_INFO("Freeing configuration resources...");
    free_config_resources(&config);
    LOG_INFO("Configuration resources freed.");

    LOG_INFO("--- Elasticsearch End-to-End Indexing Test Complete ---");
    LOG_INFO("Please check Elasticsearch index '%s' (or the plugin's hardcoded default) for the document.", config.elasticsearch_index_name);
    LOG_INFO("If ES was not running, errors related to 'Couldn't connect to server' are expected from the indexer plugin.");

    spindex_log_shutdown();
    return 0;
}
