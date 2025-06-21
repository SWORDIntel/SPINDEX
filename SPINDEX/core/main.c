#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for strlen etc. if not pulled by others

// Order matters for type completeness: threading.h provides ThreadPool for ingestion_pipeline.h
#include "threading.h"  // For ThreadPool (used in SpindexIngestionPipeline)
#include "utils.h"      // For LOG_INFO, LOG_ERROR, spindex_log_init
#include "config.h"     // For SpindexConfig definition
#include "config_manager.h" // For parse_config_file, set_default_config etc.
#include "plugin_loader.h"  // For load_plugins, unload_plugins
#include "ingestion_pipeline.h" // For SpindexIngestionPipeline, pipeline_init etc.

#define MAIN_LOG_LEVEL LOG_LEVEL_INFO // Define desired log level for main

int main(int argc, char *argv[]) {
    // Initialize configuration with defaults first
    SpindexConfig config;
    set_default_config(&config); // Apply defaults, including default log path and level

    // Initialize logger using potential default values from config
    // This allows config to be the single source of truth for defaults if parse_config_file fails
    // or is not called yet.
    spindex_log_init(config.log_file_path, config.log_level);
    LOG_INFO("Spindex Application Starting... Initial log path: %s, Initial log level: %d", config.log_file_path, config.log_level);

    if (argc != 2) {
        LOG_ERROR("Usage: %s <config_file_path>", argv[0]);
        fprintf(stderr, "Usage: %s <config_file_path>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *config_file_path = argv[1];
    LOG_INFO("Configuration file specified: %s", config_file_path);

    // 1. Initialize and Load Configuration
    SpindexConfig config;
    set_default_config(&config); // Apply defaults first

    // Potentially override default log path if specified in config and logger can be re-init or takes path from config struct
    // For now, we use a default log path set above.
    // If config.log_file_path is populated by set_default_config or parse_config_file, logger could use it.
    // Example: spindex_logger_init(config.log_file_path, config.log_level); (after parsing)

    // Store initial log settings to check if they change after parsing config
    char initial_log_path[MAX_CONFIG_STR_LEN];
    strncpy(initial_log_path, config.log_file_path, MAX_CONFIG_STR_LEN -1);
    initial_log_path[MAX_CONFIG_STR_LEN -1] = '\0';
    LogLevel initial_log_level = config.log_level;

    if (parse_config_file(config_file_path, &config) != 0) {
        LOG_ERROR("Failed to parse configuration file: %s. Using default/initial settings. Exiting.", config_file_path);
        // spindex_logger_cleanup(); // If logger needs explicit cleanup
        return EXIT_FAILURE;
    }
    LOG_INFO("Configuration loaded successfully.");

    // Check if log settings changed and re-initialize logger if necessary
    if (strcmp(initial_log_path, config.log_file_path) != 0 || initial_log_level != config.log_level) {
        LOG_INFO("Log settings changed by configuration. Re-initializing logger with path: %s, level: %d",
                 config.log_file_path, config.log_level);
        spindex_log_shutdown(); // Close existing log file
        spindex_log_init(config.log_file_path, config.log_level); // Re-initialize with new settings
        LOG_INFO("Logger re-initialized with new settings from config file.");
    }

    if (config.log_level <= LOG_LEVEL_DEBUG) { // Use config.log_level now
        print_config(&config);
    }

    // 2. Load Plugins
    SpindexPlugin *loaded_plugins = NULL;
    int plugin_count = 0;
    LOG_INFO("Loading plugins from directory: %s", config.plugin_directory);
    if (load_plugins(config.plugin_directory, &loaded_plugins, &plugin_count) != 0) {
        LOG_ERROR("Failed to load plugins. Continuing with core functionality if possible, or exiting.");
        // Depending on requirements, might exit or continue without plugins
        // For now, let's try to continue if pipeline_init can handle 0 plugins for some operations
    }
    if (plugin_count > 0) {
        LOG_INFO("Successfully loaded %d plugins.", plugin_count);
    } else {
        LOG_WARN("No plugins were loaded. Pipeline functionality might be limited.");
    }

    // 3. Initialize Ingestion Pipeline
    SpindexIngestionPipeline pipeline;
    LOG_INFO("Initializing ingestion pipeline...");
    if (pipeline_init(&pipeline, &config, loaded_plugins, plugin_count) != 0) {
        LOG_ERROR("Failed to initialize ingestion pipeline. Exiting.");
        if (loaded_plugins) {
            unload_plugins(loaded_plugins, plugin_count);
        }
        // free_config_resources(&config); // If config had dynamic parts
        // spindex_logger_cleanup();
        return EXIT_FAILURE;
    }
    LOG_INFO("Ingestion pipeline initialized successfully.");

    // 4. Simulate processing a list of files
    const char* files_to_process[] = {
        "test_data/sample.json", // Make sure this path is relative to where spindex_app is run
        "test_data/sample.txt",
        "test_data/non_existent_file.txt", // To test error handling
        "test_data/sample_unknown.ext" // To test unsupported extension
        // Add more files as needed, perhaps read from config or a directory
    };
    int num_files_to_process = sizeof(files_to_process) / sizeof(files_to_process[0]);

    LOG_INFO("Submitting %d files for processing...", num_files_to_process);
    for (int i = 0; i < num_files_to_process; ++i) {
        // Check if file exists before submitting, or let the pipeline handle it
        // For robustness, a check here could be good:
        // struct stat buffer;
        // if (stat(files_to_process[i], &buffer) == 0) {
        LOG_INFO("Submitting file: %s", files_to_process[i]);
        if (pipeline_submit_file_for_processing(&pipeline, files_to_process[i]) != 0) {
            LOG_ERROR("Failed to submit file %s for processing.", files_to_process[i]);
        }
        // } else {
        //    LOG_WARN("File not found, skipping submission: %s", files_to_process[i]);
        // }
    }

    LOG_INFO("All file processing tasks submitted.");
    LOG_INFO("Application will now wait for tasks to complete by shutting down the pipeline...");

    // 5. Shutdown Pipeline (waits for tasks) and Unload Plugins
    LOG_INFO("Shutting down ingestion pipeline...");
    pipeline_shutdown(&pipeline); // This will wait for threads to finish
    LOG_INFO("Ingestion pipeline shut down.");

    if (loaded_plugins) {
        LOG_INFO("Unloading plugins...");
        unload_plugins(loaded_plugins, plugin_count);
        LOG_INFO("Plugins unloaded.");
    }

    // 6. Free Config Resources (if any were dynamic)
    // free_config_resources(&config); // Currently, config struct is simple.

    LOG_INFO("Spindex Application Shutting Down Gracefully.");
    // spindex_logger_cleanup(); // If logger needs explicit cleanup
    return EXIT_SUCCESS;
}
