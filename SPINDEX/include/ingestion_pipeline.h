#ifndef INGESTION_PIPELINE_H
#define INGESTION_PIPELINE_H

#include "data_types.h"       // For ProcessedDataChunk
#include "config.h"           // For SpindexConfig
#include "plugin_interface.h" // For SpindexPlugin
#include "threading.h"        // For ThreadPool
+#include "nlp_ner_parser.h"   // For NlpNerParserConfig
#include <pthread.h>          // For pthread_mutex_t

/**
 * @brief Represents the ingestion pipeline manager.
 *
 * This structure holds the context for the ingestion process, including
 * references to loaded plugins that will be used for preprocessing and parsing.
 */
typedef struct {
    SpindexPlugin *all_loaded_plugins; // Pointer to the array of all loaded plugins
    int num_all_plugins;               // Total number of loaded plugins
    const SpindexConfig *app_config;   // Reference to the application configuration

    // Active indexer details
    SpindexPlugin *active_indexer_plugin;     // Pointer to the active indexer plugin instance
    void* active_indexer_plugin_data;         // Data specific to the active indexer instance (managed by plugin)
                                              // NOTE: Current plugin_loader needs enhancement to support this properly.

    // Future fields might include:
    // - Caches for frequently used plugins
    ThreadPool thread_pool;        // Thread pool for parallel processing
+    NlpNerParserConfig ner_config; // Configuration for the NLP/NER module
    pthread_mutex_t indexer_mutex; // To protect calls to non-thread-safe indexer plugins
    // - Statistics on processed files/data
} SpindexIngestionPipeline;

/**
 * @brief Initializes the ingestion pipeline.
 *
 * @param pipeline Pointer to the SpindexIngestionPipeline struct to initialize.
 * @param config Pointer to the application's configuration.
 * @param loaded_plugins Pointer to an array of already loaded SpindexPlugin instances.
 * @param num_plugins Number of plugins in the loaded_plugins array.
 * @return 0 on success, -1 on failure.
 */
int pipeline_init(SpindexIngestionPipeline *pipeline,
                  const SpindexConfig *config,
                  SpindexPlugin *loaded_plugins,
                  int num_plugins);

/**
 * @brief Processes a single file through the ingestion pipeline.
 *
 * This function will:
 * 1. Determine the file type (e.g., by extension).
 * 2. Find and invoke an appropriate preprocessor plugin.
 * 3. Find and invoke an appropriate parser plugin on the preprocessed data.
 * 4. (Future) Pass the parsed data to an indexer or other consumer.
 *
 * @param pipeline Pointer to the initialized SpindexIngestionPipeline.
 * @param file_path Path to the file to be processed.
 * @return ProcessedDataChunk* on success (caller must free this chunk),
 *         NULL on failure (e.g., file not found, unsupported file type,
 *         preprocessor/parser failed). Errors are logged internally.
 */
ProcessedDataChunk* pipeline_process_file(SpindexIngestionPipeline *pipeline, const char *file_path);

/**
 * @brief Frees a ProcessedDataChunk, including its data and source_id if allocated.
 *
 * This is a helper function that might be used by consumers of chunks
 * returned by the pipeline.
 *
 * @param chunk Pointer to the ProcessedDataChunk to be freed.
 */
void free_processed_data_chunk(ProcessedDataChunk *chunk);

/**
 * @brief Submits a file for processing by the ingestion pipeline's thread pool.
 *
 * This function is asynchronous. It creates a task for the given file_path
 * and submits it to the internal thread pool. The actual processing (preprocessing,
 * parsing, indexing) will happen on a worker thread.
 *
 * @param pipeline Pointer to the initialized SpindexIngestionPipeline.
 * @param file_path Path to the file to be processed.
 * @return 0 on successful submission, -1 on failure (e.g., memory allocation error,
 *         thread pool submission error). Errors are logged internally.
 */
int pipeline_submit_file_for_processing(SpindexIngestionPipeline* pipeline, const char* file_path);

/**
 * @brief Shuts down the ingestion pipeline and releases its resources.
 *
 * Note: This function does NOT unload the plugins themselves, as that is the
 * responsibility of the plugin_loader module. It only cleans up resources
 * specifically allocated by the pipeline.
 *
 * @param pipeline Pointer to the SpindexIngestionPipeline struct.
 */
void pipeline_shutdown(SpindexIngestionPipeline *pipeline);

#endif // INGESTION_PIPELINE_H
