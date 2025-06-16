#define _GNU_SOURCE // For strdup
#include "threading.h"
#include "ingestion_pipeline.h"
#include "utils.h" // For logging
#include "nlp_ner_parser.h"           // For NLP/NER functionality
#include "geometric_validator.h"      // For geometric validation
#include "complex_geometry_handler.h" // For complex geometry handling
#include "geojson_processor.h"        // For the new GeoJSON processing orchestrator
#include <pthread.h>                  // For pthread_self in task wrapper, and mutex ops
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For checking file existence/size
#include <errno.h>    // For errno

// Helper function to get file extension (very basic)
static const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return ""; // No extension or dot is first char
    return dot + 1;
}

// Helper function to get compression type from file extension
static const char* get_compression_type_from_extension(const char *filename) {
    const char *ext = get_file_extension(filename);
    if (ext == NULL || *ext == '\0') return NULL; // No valid extension
    if (strcmp(ext, "zip") == 0) return "ZIP";
    if (strcmp(ext, "gz") == 0 || strcmp(ext, "gzip") == 0) return "GZIP";
    if (strcmp(ext, "zst") == 0 || strcmp(ext, "zstd") == 0) return "ZSTD";
    return NULL;
}

// Helper function to find a decompressor plugin by compression type string
static SpindexPlugin* find_decompressor_plugin(const char *compression_type, SpindexPlugin *plugins, int num_plugins) {
    if (!compression_type) return NULL;
    for (int i = 0; i < num_plugins; i++) {
        if (plugins[i].info.type == SPINDEX_PLUGIN_TYPE_DECOMPRESSOR) {
            if (strstr(plugins[i].info.name, compression_type) != NULL) {
                LOG_DEBUG("Found decompressor plugin '%s' for type '%s'", plugins[i].info.name, compression_type);
                return &plugins[i];
            }
        }
    }
    LOG_DEBUG("No decompressor plugin found for compression type: %s", compression_type);
    return NULL;
}

// Helper function to find a plugin by name
static SpindexPlugin* find_plugin_by_name(const char *name, SpindexPlugin *plugins, int num_plugins) {
    for (int i = 0; i < num_plugins; i++) {
        if (strcmp(plugins[i].info.name, name) == 0) {
            return &plugins[i];
        }
    }
    return NULL;
}

// Helper function to read entire file into a buffer
static ProcessedDataChunk* read_file_to_chunk(const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file: %s", file_path);
        return NULL;
    }
    struct stat st;
    if (stat(file_path, &st) != 0) {
        LOG_ERROR("Failed to get file stats: %s", file_path);
        fclose(file);
        return NULL;
    }
    long file_size = st.st_size;
    ProcessedDataChunk *chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!chunk) {
        LOG_ERROR("Failed to allocate memory for ProcessedDataChunk.");
        fclose(file);
        return NULL;
    }
    chunk->data_type = CHUNK_TYPE_RAW_BYTES;
    chunk->data = malloc(file_size + 1);
    if (!chunk->data) {
        LOG_ERROR("Failed to allocate memory for file content (size: %ld).", file_size);
        free(chunk);
        fclose(file);
        return NULL;
    }
    size_t bytes_read = fread(chunk->data, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        LOG_ERROR("Failed to read full file content. Expected %ld, got %zu.", file_size, bytes_read);
        free(chunk->data);
        free(chunk);
        fclose(file);
        return NULL;
    }
    ((char*)chunk->data)[file_size] = '\0';
    chunk->length = file_size;
    if (bytes_read > 0) {
        chunk->data_type = CHUNK_TYPE_UTF8_TEXT;
    }
    chunk->source_id = strdup(file_path);
    if (!chunk->source_id) {
        LOG_WARN("Failed to allocate memory for chunk source_id. Continuing without it.");
    }
    fclose(file);
    LOG_DEBUG("Successfully read file '%s' into chunk, size %zu bytes, type %d.", file_path, chunk->length, chunk->data_type);
    return chunk;
}

void free_processed_data_chunk(ProcessedDataChunk *chunk) {
    if (!chunk) return;
    if (chunk->data) free(chunk->data);
    if (chunk->source_id) free(chunk->source_id);
    free(chunk);
}

static void initialize_ner_config(SpindexIngestionPipeline *pipeline) {
    if (load_nlp_ner_parser_config(pipeline->app_config, &pipeline->ner_config) != 0) {
        LOG_WARN("Failed to load NLP/NER parser configuration. NLP/NER will be unavailable.");
        pipeline->ner_config.enabled = 0; // Ensure it's marked as disabled
    } else {
        LOG_INFO("NLP/NER parser configuration loaded. Enabled: %d", pipeline->ner_config.enabled);
    }
}

// --- Thread Pool Task Definitions ---

// Argument structure for file processing tasks
typedef struct {
    SpindexIngestionPipeline* pipeline;
    char* file_path; // Needs to be duplicated for each task
} FileProcessingTaskArgs;

// Task function to be executed by worker threads
static void process_file_task_wrapper(void* arg) {
    FileProcessingTaskArgs* task_args = (FileProcessingTaskArgs*)arg;
    char log_file_path_buffer[1024] = {0};

    if (!task_args || !task_args->pipeline || !task_args->file_path) {
        LOG_ERROR("process_file_task_wrapper: Invalid task arguments received by thread %lu.", (unsigned long)pthread_self());
        if (task_args) {
            if (task_args->file_path) {
                strncpy(log_file_path_buffer, task_args->file_path, sizeof(log_file_path_buffer) - 1);
                log_file_path_buffer[sizeof(log_file_path_buffer) - 1] = '\0'; // Ensure null termination
                free(task_args->file_path);
            }
            free(task_args);
        }
        // Avoid using log_file_path_buffer if task_args->file_path was NULL initially
        LOG_ERROR("Invalid task args for thread %lu, file path (if available and copied): %s", (unsigned long)pthread_self(), log_file_path_buffer[0] ? log_file_path_buffer : "UNKNOWN");
        return;
    }

    strncpy(log_file_path_buffer, task_args->file_path, sizeof(log_file_path_buffer) - 1);
    log_file_path_buffer[sizeof(log_file_path_buffer) - 1] = '\0';

    LOG_INFO("Thread %lu: Starting task for file: %s", (unsigned long)pthread_self(), log_file_path_buffer);

    ProcessedDataChunk* result_chunk = pipeline_process_file(task_args->pipeline, task_args->file_path);

    if (result_chunk) {
        LOG_INFO("Thread %lu: File %s processed, result chunk returned (no active indexer). Freeing chunk.", (unsigned long)pthread_self(), log_file_path_buffer);
        free_processed_data_chunk(result_chunk);
    } else {
        LOG_INFO("Thread %lu: Task for file %s completed (data indexed or error during processing).", (unsigned long)pthread_self(), log_file_path_buffer);
    }

    free(task_args->file_path);
    task_args->file_path = NULL;
    free(task_args);
    LOG_DEBUG("Thread %lu: Finished and cleaned up task for file: %s", (unsigned long)pthread_self(), log_file_path_buffer);
}

// --- End Thread Pool Task Definitions ---

int pipeline_init(SpindexIngestionPipeline *pipeline,
                  const SpindexConfig *config,
                  SpindexPlugin *loaded_plugins,
                  int num_plugins) {
    if (!pipeline || !config || !loaded_plugins) {
        LOG_ERROR("Pipeline init received NULL arguments.");
        return -1;
    }

    pipeline->app_config = config;
    pipeline->all_loaded_plugins = loaded_plugins;
    pipeline->num_all_plugins = num_plugins;
    pipeline->active_indexer_plugin = NULL;
    pipeline->active_indexer_plugin_data = NULL;

    size_t num_threads = 4;
    if (thread_pool_init(&pipeline->thread_pool, num_threads) != 0) {
        LOG_ERROR("Failed to initialize thread pool for ingestion pipeline.");
        return -1;
    }
    LOG_INFO("Thread pool initialized successfully with %zu threads.", num_threads);

    if (pthread_mutex_init(&pipeline->indexer_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize indexer mutex.");
        thread_pool_destroy(&pipeline->thread_pool);
        return -1;
    }
    LOG_DEBUG("Indexer mutex initialized.");

    if (config->active_indexer_name && strlen(config->active_indexer_name) > 0) {
        LOG_INFO("Attempting to find and set active indexer: '%s'", config->active_indexer_name);
        for (int i = 0; i < num_plugins; ++i) {
            if (loaded_plugins[i].info.type == SPINDEX_PLUGIN_TYPE_INDEXER &&
                strcmp(loaded_plugins[i].info.name, config->active_indexer_name) == 0) {
                pipeline->active_indexer_plugin = &loaded_plugins[i];
                LOG_INFO("Active indexer plugin set: %s (Version: %s)",
                         pipeline->active_indexer_plugin->info.name,
                         pipeline->active_indexer_plugin->info.version);
                break;
            }
        }
        if (!pipeline->active_indexer_plugin) {
            LOG_WARN("Configured active indexer '%s' not found or not of type INDEXER.", config->active_indexer_name);
        }
    } else {
        LOG_INFO("No active indexer configured.");
    }

    // Initialize NLP/NER configuration
    initialize_ner_config(pipeline);

    // Initialize Geometric Validator
    if (geometric_validator_init() != 0) {
        LOG_ERROR("Failed to initialize geometric validator. Validation will be unavailable.");
        // Decide if this is a fatal error for the pipeline or if it can continue with validation disabled.
        // For now, assume it can continue, GeometricValidatorConfig.enabled will control actual use.
    }
    // Load configurations for sub-modules that need it from the main app_config
    if (load_complex_geometry_handler_config(pipeline->app_config, &pipeline->cg_config) != 0) {
        LOG_WARN("Failed to load Complex Geometry Handler config. Defaults may apply or functionality may be impaired.");
    }
    if (load_geometric_validator_config(pipeline->app_config, &pipeline->validator_config) != 0) {
        LOG_WARN("Failed to load Geometric Validator config. Defaults may apply or validation may be disabled.");
        pipeline->validator_config.enabled = 0; // Ensure disabled if config load fails
    }

    LOG_INFO("Ingestion pipeline initialized. Aware of %d loaded plugins. Active indexer: %s",
        num_plugins,
        pipeline->active_indexer_plugin ? pipeline->active_indexer_plugin->info.name : "None");
    return 0;
}

ProcessedDataChunk* pipeline_process_file(SpindexIngestionPipeline *pipeline, const char *file_path) {
    if (!pipeline || !file_path) {
        LOG_ERROR("pipeline_process_file: NULL pipeline or file_path.");
        return NULL;
    }
    unsigned long thread_id_for_log = (unsigned long)pthread_self();
    LOG_DEBUG("Thread %lu: Sync processing for file: %s", thread_id_for_log, file_path);

    const char *extension = get_file_extension(file_path);
    LOG_DEBUG("Thread %lu: File extension: .%s for %s", thread_id_for_log, extension, file_path);

    SpindexPlugin *preprocessor = NULL;
    SpindexPlugin *parser = NULL;
    char preproc_name[256] = {0};
    char parser_name[256] = {0};
    ProcessedDataChunk *current_chunk = NULL;


    if (strcmp(extension, "json") == 0 || strcmp(extension, "geojson") == 0) {
        // For GeoJSON, we first read the raw chunk, then pass it to the geojson_processor
        current_chunk = read_file_to_chunk(file_path);
        if (!current_chunk) {
            LOG_ERROR("Thread %lu: Failed to read initial GeoJSON file %s into chunk.", thread_id_for_log, file_path);
            return NULL;
        }
        // Define a base temporary directory for geojson processing artifacts
        const char* geojson_temp_base = "./temp_geojson_processing";
        struct stat st_temp_base = {0};
        if (stat(geojson_temp_base, &st_temp_base) == -1) {
            if (mkdir(geojson_temp_base, 0700) != 0 && errno != EEXIST) {
                LOG_ERROR("Thread %lu: Failed to create base temp directory for GeoJSON: %s. Error: %s", thread_id_for_log, geojson_temp_base, strerror(errno));
                free_processed_data_chunk(current_chunk);
                return NULL;
            }
        }

        LOG_INFO("Thread %lu: Processing GeoJSON file %s with geojson_processor.", thread_id_for_log, file_path);
        AEGIS_PROCESSING_STATUS geo_status = process_geojson_chunk(
            &current_chunk,
            pipeline->app_config,
            &pipeline->cg_config,
            &pipeline->validator_config,
            geojson_temp_base
        );

        if (geo_status != AEGIS_PROCESSING_SUCCESS && geo_status != AEGIS_PROCESSING_WARNING_GEOMETRY_INVALID) {
            LOG_ERROR("Thread %lu: geojson_processor failed for %s with status %d.", thread_id_for_log, file_path, geo_status);
            free_processed_data_chunk(current_chunk); // current_chunk might be NULL if process_geojson_chunk freed it on error
            return NULL;
        }
        // If geo_status is SUCCESS or WARNING_GEOMETRY_INVALID, current_chunk is now the (potentially) modified chunk.
        // Preprocessor and parser names are not used for this path as geojson_processor handles it.
        preproc_name[0] = '\0';
        parser_name[0] = '\0';

    } else if (strcmp(extension, "txt") == 0) {
        strcpy(preproc_name, "TXTPreprocessor");
        strcpy(parser_name, "TXTParser");
    } else if (strcmp(extension, "csv") == 0) {
        strcpy(preproc_name, "CSVPreprocessor");
        strcpy(parser_name, "CSVParser");
    } else {
        LOG_ERROR("Thread %lu: Unsupported file extension: .%s for file %s.", thread_id_for_log, extension, file_path);
        return NULL;
    }

    // If not GeoJSON, or if GeoJSON processing already prepared current_chunk:
    if (!current_chunk) { // This means it wasn't a GeoJSON path, so read the file now.
        LOG_DEBUG("Thread %lu: Reading non-GeoJSON file %s into chunk.", thread_id_for_log, file_path);
        current_chunk = read_file_to_chunk(file_path);
        if (!current_chunk) {
            LOG_ERROR("Thread %lu: Failed to read file content from %s", thread_id_for_log, file_path);
            return NULL;
        }
    }

    // Apply decompression if applicable, only if not already handled by GeoJSON path or if it's a non-geojson file
    if (strcmp(extension, "json") != 0 && strcmp(extension, "geojson") != 0) {
        const char *compression_type = get_compression_type_from_extension(file_path);
        if (compression_type && current_chunk) {
            LOG_INFO("Thread %lu: File %s detected as potentially %s compressed.", thread_id_for_log, file_path, compression_type);
            SpindexPlugin *decompressor = find_decompressor_plugin(compression_type, pipeline->all_loaded_plugins, pipeline->num_all_plugins);
            if (decompressor && decompressor->process_data) {
                LOG_INFO("Thread %lu: Invoking decompressor '%s' for file %s", thread_id_for_log, decompressor->info.name, file_path);
                ProcessedDataChunk *chunk_after_decompression = decompressor->process_data(current_chunk);
                if (chunk_after_decompression) {
                    if (current_chunk != chunk_after_decompression) free_processed_data_chunk(current_chunk);
                    current_chunk = chunk_after_decompression;
                } else {
                    LOG_ERROR("Thread %lu: Decompressor plugin '%s' failed for %s.", thread_id_for_log, decompressor->info.name, file_path);
                    free_processed_data_chunk(current_chunk); return NULL;
                }
            }
        }
    }

    SpindexPlugin *ml_classifier = find_plugin_by_name("ML Content Classifier", pipeline->all_loaded_plugins, pipeline->num_all_plugins);
    if (ml_classifier && ml_classifier->process_data && current_chunk) {
        LOG_INFO("Thread %lu: Invoking ML Content Classifier for file %s", thread_id_for_log, file_path);
        ProcessedDataChunk *chunk_after_ml_classification = ml_classifier->process_data(current_chunk);
        if (chunk_after_ml_classification) {
            if (current_chunk != chunk_after_ml_classification) free_processed_data_chunk(current_chunk);
            current_chunk = chunk_after_ml_classification;
        } else {
            LOG_WARN("Thread %lu: ML Content Classifier plugin failed or returned NULL for %s.", thread_id_for_log, file_path);
        }
    }

    if (strlen(preproc_name) == 0 && strlen(parser_name) == 0) {
        LOG_DEBUG("Thread %lu: No preprocessor/parser determined for %s (extension .%s)", thread_id_for_log, file_path, extension);
        if (pipeline->active_indexer_plugin && pipeline->active_indexer_plugin->process_data && current_chunk) {
             LOG_INFO("Thread %lu: Passing raw/decompressed/geoprocessed data of file type '%s' to indexer '%s'.", thread_id_for_log, extension, pipeline->active_indexer_plugin->info.name);
        } else if (current_chunk) {
             LOG_INFO("Thread %lu: No indexer for file type '%s', or data processing failed. Discarding chunk.", thread_id_for_log, extension);
             free_processed_data_chunk(current_chunk); 
             return NULL;
        }
    }

    if (strlen(preproc_name) > 0) {
        preprocessor = find_plugin_by_name(preproc_name, pipeline->all_loaded_plugins, pipeline->num_all_plugins);
        if (!preprocessor) {
            LOG_ERROR("Thread %lu: Preprocessor plugin '%s' not found for extension '%s'.", thread_id_for_log, preproc_name, extension);
            free_processed_data_chunk(current_chunk); return NULL;
        }
    }

    if (strlen(parser_name) > 0) {
        parser = find_plugin_by_name(parser_name, pipeline->all_loaded_plugins, pipeline->num_all_plugins);
         if (!parser) {
            LOG_ERROR("Thread %lu: Parser plugin '%s' not found for extension '%s'.", thread_id_for_log, parser_name, extension);
            free_processed_data_chunk(current_chunk); return NULL;
        }
    }

    if (preprocessor && preprocessor->process_data && current_chunk) {
        LOG_INFO("Thread %lu: Invoking preprocessor '%s' for file %s", thread_id_for_log, preprocessor->info.name, file_path);
        ProcessedDataChunk *chunk_after_preproc = preprocessor->process_data(current_chunk);
        if (!chunk_after_preproc) {
            LOG_ERROR("Thread %lu: Preprocessor plugin '%s' failed for %s.", thread_id_for_log, preprocessor->info.name, file_path);
            free_processed_data_chunk(current_chunk); return NULL;
        }
        if (chunk_after_preproc != current_chunk) free_processed_data_chunk(current_chunk);
        current_chunk = chunk_after_preproc;
    }

    if (parser && parser->process_data && current_chunk) {
        LOG_INFO("Thread %lu: Invoking parser '%s' for %s", thread_id_for_log, parser->info.name, file_path);
        ProcessedDataChunk *chunk_after_parser = parser->process_data(current_chunk);
        if (!chunk_after_parser) {
            LOG_ERROR("Thread %lu: Parser plugin '%s' failed for %s.", thread_id_for_log, parser->info.name, file_path);
            free_processed_data_chunk(current_chunk); return NULL;
        }
        if (chunk_after_parser != current_chunk) free_processed_data_chunk(current_chunk);
        current_chunk = chunk_after_parser;
    }

    // NLP/NER Processing Stage
    if (pipeline->ner_config.enabled && current_chunk && current_chunk->data_type == CHUNK_TYPE_UTF8_TEXT && current_chunk->length > 0) {
        LOG_INFO("Thread %lu: Invoking NLP/NER location extraction for %s", thread_id_for_log, file_path);
        char** extracted_locs = NULL;
        int num_extracted_locs = 0;

        // Assuming current_chunk->data is a null-terminated UTF-8 string
        AEGIS_PROCESSING_STATUS ner_status = extract_locations_from_text(
            (char*)current_chunk->data,
            &pipeline->ner_config,
            &extracted_locs,
            &num_extracted_locs
        );

        if (ner_status == AEGIS_PROCESSING_SUCCESS && num_extracted_locs > 0) {
            LOG_INFO("Thread %lu: NLP/NER extracted %d locations from %s.", thread_id_for_log, num_extracted_locs, file_path);
            for (int i = 0; i < num_extracted_locs; ++i) {
                LOG_INFO("Thread %lu: NER Location %d for %s: %s", thread_id_for_log, i + 1, file_path, extracted_locs[i]);
                // TODO: Call Geocoding Module [AEGIS-007] with extracted_locs[i] or the full list.
            }
            free_extracted_locations(extracted_locs, num_extracted_locs);
        } else if (ner_status != AEGIS_PROCESSING_SUCCESS && ner_status != AEGIS_PROCESSING_NLP_NER_DISABLED) {
            LOG_WARN("Thread %lu: NLP/NER processing for %s failed with status %d.", thread_id_for_log, file_path, ner_status);
        } else {
            LOG_DEBUG("Thread %lu: NLP/NER for %s: no locations found, module disabled, or empty text.",thread_id_for_log, file_path);
        }
    }


    LOG_INFO("Thread %lu: File %s processing stages completed. Final data length: %zu.",
             thread_id_for_log, file_path, current_chunk ? current_chunk->length : 0);

    if (pipeline->active_indexer_plugin && current_chunk) {
        LOG_INFO("Thread %lu: Passing data from %s to active indexer: %s", thread_id_for_log, file_path, pipeline->active_indexer_plugin->info.name);

        LOG_DEBUG("Thread %lu: Locking indexer mutex for file %s", thread_id_for_log, file_path);
        pthread_mutex_lock(&pipeline->indexer_mutex);
        LOG_DEBUG("Thread %lu: Indexer mutex locked for file %s", thread_id_for_log, file_path);

        ProcessedDataChunk* indexing_status_chunk = pipeline->active_indexer_plugin->process_data(current_chunk);

        LOG_DEBUG("Thread %lu: Unlocking indexer mutex for file %s", thread_id_for_log, file_path);
        pthread_mutex_unlock(&pipeline->indexer_mutex);
        LOG_DEBUG("Thread %lu: Indexer mutex unlocked for file %s", thread_id_for_log, file_path);

        if (indexing_status_chunk) {
            LOG_DEBUG("Thread %lu: Indexer for %s returned status chunk (type %d), freeing it.", thread_id_for_log, file_path, indexing_status_chunk->data_type);
            free_processed_data_chunk(indexing_status_chunk);
        }
        free_processed_data_chunk(current_chunk);
        LOG_DEBUG("Thread %lu: Finished processing for file: %s. Data passed to indexer.", thread_id_for_log, file_path);
        return NULL;
    } else if (current_chunk) {
        LOG_DEBUG("Thread %lu: Finished processing for file: %s. No active indexer. Returning final chunk.", thread_id_for_log, file_path);
        return current_chunk;
    } else {
        LOG_DEBUG("Thread %lu: Finished processing for file: %s. No data to index or return.", thread_id_for_log, file_path);
        return NULL;
    }
}

void pipeline_shutdown(SpindexIngestionPipeline *pipeline) {
    if (!pipeline) return;
    LOG_INFO("Ingestion pipeline shutting down.");

    if (pipeline->active_indexer_plugin) {
        LOG_INFO("Pipeline explicitly acknowledging shutdown for active indexer: %s (actual shutdown by plugin_loader)",
                 pipeline->active_indexer_plugin->info.name);
    }

    LOG_INFO("Shutting down thread pool for ingestion pipeline.");
    if (thread_pool_destroy(&pipeline->thread_pool) != 0) {
        LOG_ERROR("Failed to properly destroy thread pool during pipeline shutdown.");
    } else {
        LOG_INFO("Thread pool destroyed successfully.");
    }

    // Shutdown Geometric Validator
    geometric_validator_shutdown();

    LOG_DEBUG("Destroying indexer mutex.");
    if (pthread_mutex_destroy(&pipeline->indexer_mutex) != 0) {
        LOG_ERROR("Failed to destroy indexer mutex during pipeline shutdown.");
    } else {
        LOG_DEBUG("Indexer mutex destroyed successfully.");
    }

    pipeline->all_loaded_plugins = NULL;
    pipeline->num_all_plugins = 0;
    pipeline->app_config = NULL;
    pipeline->active_indexer_plugin = NULL;
}

int pipeline_submit_file_for_processing(SpindexIngestionPipeline* pipeline, const char* file_path) {
    if (!pipeline || !file_path) {
        LOG_ERROR("pipeline_submit_file_for_processing: NULL pipeline or file_path.");
        return -1;
    }
     if (pipeline->thread_pool.shutdown) {
        LOG_WARN("Attempted to submit task for file '%s' but thread pool is shutting down.", file_path);
        return -1;
    }

    // Ensure NER config is loaded. This is slightly redundant if pipeline_init always does it,
    // but safe if submit can be called after a config reload that pipeline_init didn't see.
    // A better pattern might be a dedicated reconfigure function.
    if(pipeline->app_config && pipeline->ner_config.python_executable[0] == '\0') { // Crude check if not initialized
        initialize_ner_config(pipeline);
    }

    FileProcessingTaskArgs* task_args = (FileProcessingTaskArgs*)malloc(sizeof(FileProcessingTaskArgs));
    if (!task_args) {
        LOG_ERROR("Failed to allocate memory for task arguments for file: %s", file_path);
        return -1;
    }

    task_args->pipeline = pipeline;
    task_args->file_path = strdup(file_path);
    if (!task_args->file_path) {
        LOG_ERROR("Failed to duplicate file_path for task arguments: %s", file_path);
        free(task_args);
        return -1;
    }

    if (thread_pool_submit(&pipeline->thread_pool, process_file_task_wrapper, task_args) != 0) {
        LOG_ERROR("Failed to submit file processing task to thread pool for: %s", file_path);
        free(task_args->file_path);
        free(task_args);
        return -1;
    }
    return 0;
}