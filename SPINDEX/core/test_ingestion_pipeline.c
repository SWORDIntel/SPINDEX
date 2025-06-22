#define _GNU_SOURCE // For strdup
#include "utils.h"
#include "config_manager.h"
#include "plugin_loader.h"
#include "threading.h" // Must be before ingestion_pipeline.h if it uses ThreadPool type
#include "ingestion_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
    printf("--- Starting Ingestion Pipeline Test (with TXT, CSV, and Indexer Handling) ---\n");

    spindex_log_init("test_ingestion_pipeline.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Test program started. Logger initialized.");

    SpindexConfig config;
    memset(&config, 0, sizeof(SpindexConfig));

    if (parse_config_file("test_spindex_es.conf", &config) != 0) {
        LOG_WARN("Could not load 'test_spindex_es.conf'. Using default settings and overriding for test.");
        set_default_config(&config);
        snprintf(config.plugin_directory, MAX_CONFIG_STR_LEN, "/app/SPINDEX/build/plugins_for_test");
        if (config.active_indexer_name) free(config.active_indexer_name);
        config.active_indexer_name = strdup("ElasticsearchIndexer");
    } else {
        LOG_INFO("Successfully loaded 'test_spindex_es.conf'.");
        snprintf(config.plugin_directory, MAX_CONFIG_STR_LEN, "/app/SPINDEX/build/plugins_for_test");
        LOG_INFO("Overriding plugin_directory for test: %s", config.plugin_directory);
    }

    LOG_INFO("Initial configuration for test run:");
    print_config(&config);

    char* original_active_indexer_name = config.active_indexer_name ? strdup(config.active_indexer_name) : NULL;

    SpindexPlugin *loaded_plugins = NULL;
    int plugin_count = 0;
    LOG_INFO("Loading plugins from: %s", config.plugin_directory);
    if (load_plugins(config.plugin_directory, &loaded_plugins, &plugin_count) != 0) {
        LOG_ERROR("Failed to load plugins. Aborting test.");
        if(original_active_indexer_name) free(original_active_indexer_name);
        free_config_resources(&config);
        spindex_log_shutdown();
        exit(1); // Added exit
    }
    if (plugin_count == 0) {
        LOG_ERROR("No plugins were loaded from %s. Aborting test.", config.plugin_directory);
        if(original_active_indexer_name) free(original_active_indexer_name);
        unload_plugins(loaded_plugins, plugin_count);
        free_config_resources(&config);
        spindex_log_shutdown();
        exit(1); // Added exit
    }
    LOG_INFO("%d plugin(s) loaded successfully.", plugin_count);
     for(int i=0; i<plugin_count; ++i) {
        LOG_DEBUG("Loaded Plugin %d: Name=%s, Version=%s, Type=%d, Desc=%s",
                  i+1, loaded_plugins[i].info.name, loaded_plugins[i].info.version,
                  loaded_plugins[i].info.type, loaded_plugins[i].info.description);
    }

    SpindexIngestionPipeline pipeline_instance;

    LOG_INFO("--- Testing JSON file processing (indexer: %s) ---",
             config.active_indexer_name ? config.active_indexer_name : "None");
    if (pipeline_init(&pipeline_instance, &config, loaded_plugins, plugin_count) != 0) {
        LOG_ERROR("Failed to initialize pipeline for JSON test. Aborting.");
        if(original_active_indexer_name) free(original_active_indexer_name);
        unload_plugins(loaded_plugins, plugin_count);
        free_config_resources(&config);
        spindex_log_shutdown();
        exit(1); // Added exit
    }
    const char *json_test_file_path = "/app/SPINDEX/test_data/sample_specific.json";
    ProcessedDataChunk *json_pipeline_output = pipeline_process_file(&pipeline_instance, json_test_file_path);
    if (pipeline_instance.active_indexer_plugin) {
        assert(json_pipeline_output == NULL && "Pipeline should return NULL when indexer is active and consumes data for JSON.");
        LOG_INFO("JSON Test: Pipeline returned NULL as expected (data consumed by indexer '%s').",
                 pipeline_instance.active_indexer_plugin->info.name);
    } else {
        assert(json_pipeline_output != NULL && "Pipeline should return data when no indexer is active for JSON.");
        if (json_pipeline_output) {
            LOG_INFO("JSON Test: No active indexer. Pipeline returned chunk (type %d). Verifying...", json_pipeline_output->data_type);
            assert(json_pipeline_output->data_type == CHUNK_TYPE_STRUCTURED_DATA);
            assert(json_pipeline_output->data != NULL);
            assert(strncmp((char*)json_pipeline_output->data, "Parsed: ", strlen("Parsed: ")) == 0);
            free_processed_data_chunk(json_pipeline_output);
        }
    }
    pipeline_shutdown(&pipeline_instance);

    LOG_INFO("--- Testing TXT file processing (disabling indexer to check parser output) ---");
    if(config.active_indexer_name) free(config.active_indexer_name);
    config.active_indexer_name = NULL;

    LOG_INFO("Re-initializing pipeline with indexer disabled in config for TXT test...");
    if (pipeline_init(&pipeline_instance, &config, loaded_plugins, plugin_count) != 0) {
        LOG_ERROR("Failed to re-initialize pipeline for TXT test. Aborting.");
        if(original_active_indexer_name) free(original_active_indexer_name);
        unload_plugins(loaded_plugins, plugin_count);
        free_config_resources(&config);
        spindex_log_shutdown();
        exit(1); // Added exit
    }
    const char *txt_test_file_path = "/app/SPINDEX/test_data/sample.txt";
    ProcessedDataChunk *txt_pipeline_output = pipeline_process_file(&pipeline_instance, txt_test_file_path);
    assert(txt_pipeline_output != NULL && "TXT processing should return a chunk when indexer is disabled.");
    if (txt_pipeline_output) {
        LOG_INFO("TXT Test: Pipeline returned chunk (type %d, length %zu). Verifying...",
                 txt_pipeline_output->data_type, txt_pipeline_output->length);
        assert(txt_pipeline_output->data_type == CHUNK_TYPE_TEXT_PARSED);
        assert(txt_pipeline_output->data != NULL);
        LOG_DEBUG("TXT Parser Output (full): %s", (char*)txt_pipeline_output->data);
        assert(strstr((char*)txt_pipeline_output->data, "{\"text_content\":\"This is a sample text file.\\nIt has multiple lines.\\n  Some lines have leading/trailing whitespace.  \\nThe end.\"}") != NULL);
        free_processed_data_chunk(txt_pipeline_output);
    }
    pipeline_shutdown(&pipeline_instance);

    LOG_INFO("--- Testing CSV file processing (indexer still disabled) ---");
    LOG_INFO("Re-initializing pipeline with indexer disabled in config for CSV test...");
    if (pipeline_init(&pipeline_instance, &config, loaded_plugins, plugin_count) != 0) {
        LOG_ERROR("Failed to re-initialize pipeline for CSV test. Aborting.");
        if(original_active_indexer_name) free(original_active_indexer_name);
        unload_plugins(loaded_plugins, plugin_count);
        free_config_resources(&config);
        spindex_log_shutdown();
        exit(1); // Added exit
    }
    const char *csv_test_file_path = "/app/SPINDEX/test_data/sample.csv";
    ProcessedDataChunk *csv_pipeline_output = pipeline_process_file(&pipeline_instance, csv_test_file_path);
    assert(csv_pipeline_output != NULL && "CSV processing should return a chunk when indexer is disabled.");
    if (csv_pipeline_output) {
        LOG_INFO("CSV Test: Pipeline returned chunk (type %d, length %zu). Verifying...",
                 csv_pipeline_output->data_type, csv_pipeline_output->length);
        assert(csv_pipeline_output->data_type == CHUNK_TYPE_CSV_PARSED);
        assert(csv_pipeline_output->data != NULL);
        LOG_DEBUG("CSV Parser Output (full): %s", (char*)csv_pipeline_output->data);
        assert(strstr((char*)csv_pipeline_output->data, "[{\"ID\":\"1\",\"Name\":\"ItemA\",\"Value\":\"100\",\"Description\":\"First item with some details\"}") != NULL);
        assert(strstr((char*)csv_pipeline_output->data, "{\"ID\":\"3\",\"Name\":\"ItemC\",\"Value\":\"75\",\"Description\":\"A third entry for testing\"}]") != NULL);
        free_processed_data_chunk(csv_pipeline_output);
    }

    if(original_active_indexer_name){
        if(config.active_indexer_name) free(config.active_indexer_name);
        config.active_indexer_name = original_active_indexer_name;
        LOG_INFO("Restored active_indexer_name in config to: %s", config.active_indexer_name);
        pipeline_shutdown(&pipeline_instance);
        LOG_INFO("Re-initializing pipeline with original indexer config for final shutdown...");
        if (pipeline_init(&pipeline_instance, &config, loaded_plugins, plugin_count) != 0) {
             LOG_ERROR("Failed to re-initialize pipeline for final shutdown. Potential resource issues.");
             // Not exiting here, will proceed to main shutdown path
        }
    } else {
         if(config.active_indexer_name) {
            free(config.active_indexer_name);
            config.active_indexer_name = NULL;
         }
    }

    LOG_INFO("Shutting down final ingestion pipeline instance...");
    pipeline_shutdown(&pipeline_instance);

    LOG_INFO("Unloading plugins...");
    unload_plugins(loaded_plugins, plugin_count);
    LOG_INFO("Plugins unloaded.");

    LOG_INFO("Freeing configuration resources...");
    free_config_resources(&config);
    LOG_INFO("Configuration resources freed.");

    LOG_INFO("--- Ingestion Pipeline Test (with TXT, CSV) Complete ---");
    spindex_log_shutdown();
    printf("\n--- Test run complete. Check 'test_ingestion_pipeline.log' ---\n");
    return 0;
}
