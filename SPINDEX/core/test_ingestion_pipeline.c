#include "ingestion_pipeline.h"
#include "utils.h"
#include "config_manager.h"
#include "plugin_loader.h"
#include <stdio.h>
#include <string.h> // For strncmp, strlen, strstr

int main() {
    printf("--- Starting Ingestion Pipeline Test ---\n");

    // 1. Initialize Logger (using default stdout for this test for simplicity)
    // Log file for pipeline test itself, distinct from app's log config.
    spindex_log_init("test_pipeline_run.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Test program started. Logger initialized.");

    // 2. Setup Configuration
    SpindexConfig config;
    set_default_config(&config); // Load defaults
    // Modify defaults if needed for the test.
    // The Makefile now stages plugins needed for this test into TEST_PLUGINS_STAGING_DIR.
    snprintf(config.plugin_directory, MAX_CONFIG_STR_LEN, "/app/SPINDEX/build/plugins_for_test");
    LOG_INFO("Configuration prepared. Plugin directory for test: %s", config.plugin_directory);

    // 3. Load Plugins
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
        unload_plugins(loaded_plugins, plugin_count); // Handles NULL or count = 0
        spindex_log_shutdown();
        return 1;
    }
    LOG_INFO("%d plugin(s) loaded successfully.", plugin_count);
    for(int i=0; i<plugin_count; ++i) {
        LOG_DEBUG("Loaded Plugin %d: Name=%s, Version=%s", i+1, loaded_plugins[i].info.name, loaded_plugins[i].info.version);
    }


    // 4. Initialize Ingestion Pipeline
    SpindexIngestionPipeline pipeline;
    LOG_INFO("Initializing ingestion pipeline...");
    if (pipeline_init(&pipeline, &config, loaded_plugins, plugin_count) != 0) {
        LOG_ERROR("Failed to initialize ingestion pipeline. Aborting.");
        unload_plugins(loaded_plugins, plugin_count);
        spindex_log_shutdown();
        return 1;
    }
    LOG_INFO("Ingestion pipeline initialized successfully.");

    // 5. Process a specific JSON test file
    const char *json_test_file_path = "/app/SPINDEX/test_data/sample_specific.json";
    LOG_INFO("Attempting to process JSON test file: %s", json_test_file_path);
    ProcessedDataChunk *json_output_chunk = pipeline_process_file(&pipeline, json_test_file_path);

    if (json_output_chunk) {
        LOG_INFO("Successfully processed JSON file '%s'. Verifying output...", json_test_file_path);
        if (json_output_chunk->data_type == CHUNK_TYPE_PARSED_JSON_PLACEHOLDER) {
            LOG_INFO("Output chunk data_type is CHUNK_TYPE_PARSED_JSON_PLACEHOLDER (Correct).");
        } else {
            LOG_ERROR("Output chunk data_type is %d, expected CHUNK_TYPE_PARSED_JSON_PLACEHOLDER (%d)!",
                      json_output_chunk->data_type, CHUNK_TYPE_PARSED_JSON_PLACEHOLDER);
        }

        const char* expected_prefix = "Parsed: ";
        if (strncmp((char*)json_output_chunk->data, expected_prefix, strlen(expected_prefix)) == 0) {
            LOG_INFO("Output data starts with '%s' (Correct).", expected_prefix);
        } else {
            LOG_ERROR("Output data does not start with '%s'. Actual (first 30 bytes): '%.30s'",
                      expected_prefix, (char*)json_output_chunk->data);
        }

        if (json_output_chunk->source_id &&
            strstr(json_output_chunk->source_id, "JSONPreprocessor") &&
            strstr(json_output_chunk->source_id, "JSONParser")) {
            LOG_INFO("Output source_id '%s' contains preprocessor and parser names (Correct).", json_output_chunk->source_id);
        } else {
            LOG_ERROR("Output source_id '%s' does not seem correct.",
                      json_output_chunk->source_id ? json_output_chunk->source_id : "NULL");
        }
        free_processed_data_chunk(json_output_chunk); // Clean up
    } else {
        LOG_ERROR("Failed to process JSON file '%s'. Pipeline returned NULL.", json_test_file_path);
    }

    // Test with a .txt file (will use DummyPlugin as placeholder as per current logic)
    // Create a dummy test_file.txt
    const char *sample_txt_path = "/app/SPINDEX/test_data/sample_data.txt"; // Changed path
    FILE* txt_file = fopen(sample_txt_path, "w");
    if (txt_file) {
        fprintf(txt_file, "This is a simple text file for testing.");
        fclose(txt_file);
        LOG_INFO("Attempting to process file: %s", sample_txt_path);
        ProcessedDataChunk* txt_output_chunk = pipeline_process_file(&pipeline, sample_txt_path);
        if (txt_output_chunk) {
            LOG_INFO("Successfully processed file '%s' through pipeline.", sample_txt_path);
            // We can add checks for dummy plugin output here too if desired
            free_processed_data_chunk(txt_output_chunk);
        } else {
            LOG_ERROR("Failed to process file '%s'. Pipeline returned NULL.", sample_txt_path);
        }
    } else {
        LOG_WARN("Could not create %s for testing.", sample_txt_path);
    }


    // 6. Shutdown Pipeline
    LOG_INFO("Shutting down ingestion pipeline...");
    pipeline_shutdown(&pipeline);
    LOG_INFO("Ingestion pipeline shut down.");

    // 7. Unload Plugins
    LOG_INFO("Unloading plugins...");
    unload_plugins(loaded_plugins, plugin_count);
    LOG_INFO("Plugins unloaded.");

    // 8. Shutdown Logger
    LOG_INFO("Test program finished. Shutting down logger.");
    spindex_log_shutdown();

    printf("\n--- Ingestion Pipeline Test Complete ---\n");
    printf("Please check 'test_pipeline_run.log' for detailed logs.\n");

    return 0;
}
