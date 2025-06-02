#define _GNU_SOURCE // For strdup
#include "utils.h"
#include "config_manager.h"
#include "plugin_loader.h"
#include "ingestion_pipeline.h" // For free_processed_data_chunk
#include <stdio.h>
#include <stdlib.h> // For free
#include <string.h> // For strncmp, strlen, strstr

int main() {
    printf("--- Starting Ingestion Pipeline Test ---\n");

    spindex_log_init("test_pipeline_run.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Test program started. Logger initialized.");

    SpindexConfig config;
    memset(&config, 0, sizeof(SpindexConfig)); // Initialize struct to zero/NULL
    set_default_config(&config);
    snprintf(config.plugin_directory, MAX_CONFIG_STR_LEN, "/app/SPINDEX/build/plugins_for_test");
    if (config.active_indexer_name) free(config.active_indexer_name);
    config.active_indexer_name = strdup("ElasticsearchIndexer");
    LOG_INFO("Configuration prepared. Plugin directory for test: %s, Active Indexer: %s",
             config.plugin_directory, config.active_indexer_name);

    SpindexPlugin *loaded_plugins = NULL;
    int plugin_count = 0;
    LOG_INFO("Loading plugins from: %s", config.plugin_directory);
    if (load_plugins(config.plugin_directory, &loaded_plugins, &plugin_count) != 0) {
        LOG_ERROR("Failed to load plugins. Aborting test.");
        spindex_log_shutdown();
        return 1;
    }
    if (plugin_count == 0) {
        LOG_ERROR("No plugins were loaded from %s. Aborting test.", config.plugin_directory);
        unload_plugins(loaded_plugins, plugin_count);
        spindex_log_shutdown();
        return 1;
    }
    LOG_INFO("%d plugin(s) loaded successfully.", plugin_count);
    for(int i=0; i<plugin_count; ++i) {
        LOG_DEBUG("Loaded Plugin %d: Name=%s, Version=%s, Type=%d, Desc=%s",
                  i+1,
                  loaded_plugins[i].info.name,
                  loaded_plugins[i].info.version,
                  loaded_plugins[i].info.type,
                  loaded_plugins[i].info.description
                  );
    }

    SpindexIngestionPipeline pipeline_instance; // Changed from 'pipeline' to avoid conflict with struct name
    LOG_INFO("Initializing ingestion pipeline...");
    if (pipeline_init(&pipeline_instance, &config, loaded_plugins, plugin_count) != 0) {
        LOG_ERROR("Failed to initialize ingestion pipeline. Aborting.");
        unload_plugins(loaded_plugins, plugin_count);
        spindex_log_shutdown();
        return 1;
    }
    LOG_INFO("Ingestion pipeline initialized successfully.");

    const char *json_test_file_path = "/app/SPINDEX/test_data/sample_specific.json";
    LOG_INFO("Attempting to process JSON test file: %s", json_test_file_path);
    ProcessedDataChunk *final_chunk_from_pipeline = pipeline_process_file(&pipeline_instance, json_test_file_path);

    if (final_chunk_from_pipeline) {
        LOG_INFO("Pipeline returned a chunk for JSON file (data_type %d). This means it was NOT consumed by an indexer, or indexer passed it on.",
                 final_chunk_from_pipeline->data_type);
        if (pipeline_instance.active_indexer_plugin) {
             LOG_ERROR("JSON data was expected to be consumed by indexer '%s', but pipeline returned a chunk.",
                       pipeline_instance.active_indexer_plugin->info.name);
        } else {
            LOG_INFO("No active indexer, verifying parsed chunk from pipeline directly.");
            if (final_chunk_from_pipeline->data_type == CHUNK_TYPE_PARSED_JSON_PLACEHOLDER) {
                LOG_INFO("Output chunk data_type is CHUNK_TYPE_PARSED_JSON_PLACEHOLDER (Correct for non-indexed data).");
            } else {
                LOG_ERROR("Output chunk data_type is %d, expected CHUNK_TYPE_PARSED_JSON_PLACEHOLDER (%d)!",
                          final_chunk_from_pipeline->data_type, CHUNK_TYPE_PARSED_JSON_PLACEHOLDER);
            }
            const char* expected_prefix = "Parsed: ";
            if (strncmp((char*)final_chunk_from_pipeline->data, expected_prefix, strlen(expected_prefix)) == 0) {
                LOG_INFO("Output data starts with '%s' (Correct for non-indexed data).", expected_prefix);
            } else {
                LOG_ERROR("Output data does not start with '%s'. Actual (first 30 bytes): '%.30s'",
                          expected_prefix, (char*)final_chunk_from_pipeline->data);
            }
        }
        free_processed_data_chunk(final_chunk_from_pipeline);
    } else {
        if (pipeline_instance.active_indexer_plugin) {
            LOG_INFO("Pipeline returned NULL for JSON file. Data assumed consumed by indexer '%s' (Correct).",
                     pipeline_instance.active_indexer_plugin->info.name);
        } else {
            LOG_ERROR("Pipeline returned NULL for JSON file, but no active indexer was set. This indicates an issue in parsing/preprocessing.");
        }
    }

    const char *sample_txt_path = "/app/SPINDEX/test_data/sample_data.txt";
    FILE* txt_file = fopen(sample_txt_path, "w");
    if (txt_file) {
        fprintf(txt_file, "This is a simple text file for testing.");
        fclose(txt_file);
        LOG_INFO("Attempting to process .txt file: %s", sample_txt_path);
        ProcessedDataChunk* txt_output_chunk = pipeline_process_file(&pipeline_instance, sample_txt_path);
        if (txt_output_chunk) {
            LOG_INFO("Successfully processed .txt file '%s'. Pipeline returned a chunk (type %d).",
                     sample_txt_path, txt_output_chunk->data_type);
            free_processed_data_chunk(txt_output_chunk);
        } else {
            if (pipeline_instance.active_indexer_plugin) {
                 LOG_INFO("Pipeline returned NULL for .txt file. Data assumed consumed by indexer '%s'.",
                          pipeline_instance.active_indexer_plugin->info.name);
            } else {
                LOG_ERROR("Failed to process .txt file '%s'. Pipeline returned NULL and no indexer active.", sample_txt_path);
            }
        }
    } else {
        LOG_WARN("Could not create %s for testing.", sample_txt_path);
    }

    LOG_INFO("Shutting down ingestion pipeline...");
    pipeline_shutdown(&pipeline_instance);
    LOG_INFO("Ingestion pipeline shut down.");

    LOG_INFO("Unloading plugins...");
    unload_plugins(loaded_plugins, plugin_count);
    LOG_INFO("Plugins unloaded.");

    LOG_INFO("Test program finished. Shutting down logger.");
    spindex_log_shutdown();

    printf("\n--- Ingestion Pipeline Test Complete ---\n");
    printf("Please check 'test_pipeline_run.log' for detailed logs.\n");

    return 0;
}
