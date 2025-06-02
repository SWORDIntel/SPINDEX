#define _GNU_SOURCE // For strdup
#include "ingestion_pipeline.h"
#include "utils.h" // For logging
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For checking file existence/size

// Helper function to get file extension (very basic)
static const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return ""; // No extension or dot is first char
    return dot + 1;
}

// Helper function to find a plugin by name (simple exact match on plugin info name)
// In a real system, plugins might register capabilities (e.g., "handles_json_preprocessing")
static SpindexPlugin* find_plugin_by_name(const char *name, SpindexPlugin *plugins, int num_plugins) {
    for (int i = 0; i < num_plugins; i++) {
        if (strcmp(plugins[i].info.name, name) == 0) {
            return &plugins[i];
        }
    }
    return NULL;
}

// Helper function to read entire file into a buffer (ProcessedDataChunk)
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
    chunk->data_type = CHUNK_TYPE_RAW_BYTES; // Initial type

    chunk->data = malloc(file_size + 1); // +1 for potential null terminator if text
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

    ((char*)chunk->data)[file_size] = '\0'; // Null-terminate, assuming text data for now
    chunk->length = file_size;
    if (bytes_read > 0) { // simple heuristic for text, could be improved
        // A more robust check would involve looking at byte patterns.
        // For now, if it's not empty, assume it might be text.
        chunk->data_type = CHUNK_TYPE_UTF8_TEXT; // Assume UTF-8 text after reading
    }

    // Set source_id (filename is a good candidate)
    chunk->source_id = strdup(file_path);
    if (!chunk->source_id) {
        LOG_WARN("Failed to allocate memory for chunk source_id. Continuing without it.");
    }


    fclose(file);
    LOG_DEBUG("Successfully read file '%s' into chunk, size %zu bytes, type %d.", file_path, chunk->length, chunk->data_type);
    return chunk;
}

// Helper to free a ProcessedDataChunk, including its data and source_id if allocated
// Making this non-static so test code can use it if needed, or move to a common utils.
// For now, just non-static for test_ingestion_pipeline.c to use.
/* static */ void free_processed_data_chunk(ProcessedDataChunk *chunk) {
    if (!chunk) return;
    if (chunk->data) free(chunk->data);
    if (chunk->source_id) free(chunk->source_id); // Assuming strdup was used
    free(chunk);
}


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
    pipeline->active_indexer_plugin_data = NULL; // Needs proper handling via plugin_loader later

    if (config->active_indexer_name && strlen(config->active_indexer_name) > 0) {
        LOG_INFO("Attempting to find and set active indexer: '%s'", config->active_indexer_name);
        for (int i = 0; i < num_plugins; ++i) {
            if (loaded_plugins[i].info.type == SPINDEX_PLUGIN_TYPE_INDEXER &&
                strcmp(loaded_plugins[i].info.name, config->active_indexer_name) == 0) {

                pipeline->active_indexer_plugin = &loaded_plugins[i];
                LOG_INFO("Active indexer plugin set: %s (Version: %s)",
                         pipeline->active_indexer_plugin->info.name,
                         pipeline->active_indexer_plugin->info.version);

                // IMPORTANT NOTE: The current plugin_loader calls init() without parameters.
                // The indexer plugin (e.g. ElasticsearchIndexer) itself handles its own data via static global.
                // If plugin_loader were enhanced to pass void** to init, we would store that pointer here:
                // e.g., if (pipeline->active_indexer_plugin->init(config, &pipeline->active_indexer_plugin_data) != 0) { ... }
                // For now, active_indexer_plugin_data remains NULL at the pipeline level.
                // The plugin's process_data will use its internally managed (static global) data.
                break;
            }
        }
        if (!pipeline->active_indexer_plugin) {
            LOG_WARN("Configured active indexer '%s' not found or not of type INDEXER.", config->active_indexer_name);
        }
    } else {
        LOG_INFO("No active indexer configured.");
    }

    LOG_INFO("Ingestion pipeline initialized. Aware of %d loaded plugins. Active indexer: %s",
        num_plugins,
        pipeline->active_indexer_plugin ? pipeline->active_indexer_plugin->info.name : "None");
    LOG_INFO("Plugin directory from config: %s", config->plugin_directory);

    return 0;
}

ProcessedDataChunk* pipeline_process_file(SpindexIngestionPipeline *pipeline, const char *file_path) {
    if (!pipeline || !file_path) {
        LOG_ERROR("pipeline_process_file: NULL pipeline or file_path.");
        return NULL;
    }

    LOG_INFO("Starting processing for file: %s", file_path);

    const char *extension = get_file_extension(file_path);
    LOG_DEBUG("File extension: .%s", extension);

    SpindexPlugin *preprocessor = NULL;
    SpindexPlugin *parser = NULL;

    // Placeholder: Determine plugin names based on extension
    // This logic would be more sophisticated in a real system (e.g., config-driven, capability matching)
    char preproc_name[256] = {0};
    char parser_name[256] = {0};

    if (strcmp(extension, "json") == 0) {
        // For now, assume specific plugin names.
        strcpy(preproc_name, "JSONPreprocessor");
        strcpy(parser_name, "JSONParser");
        LOG_DEBUG("File type JSON identified. Required preprocessor: '%s', parser: '%s'", preproc_name, parser_name);
    } else if (strcmp(extension, "txt") == 0) {
        strcpy(preproc_name, "TXTPreprocessor");
        strcpy(parser_name, "TXTParser");
        LOG_DEBUG("File type TXT identified. Required preprocessor: '%s', parser: '%s'", preproc_name, parser_name);
    } else if (strcmp(extension, "csv") == 0) {
        strcpy(preproc_name, "CSVPreprocessor");
        strcpy(parser_name, "CSVParser");
        LOG_DEBUG("File type CSV identified. Required preprocessor: '%s', parser: '%s'", preproc_name, parser_name);
    } else {
        LOG_ERROR("Unsupported file extension: .%s for file %s. No specific preprocessor or parser will be used.", extension, file_path);
        // Keep preproc_name and parser_name empty.
        // This will result in no preprocessor/parser being found, and the original_chunk
        // might be passed to the indexer if one is active, or returned by the pipeline.
        // Or, return NULL here if unsupported types should not proceed.
        // For now, let it proceed to indexer with raw data if indexer is active.
        // If no indexer, raw data will be returned.
        // A better approach might be to return NULL immediately.
        // For this iteration, let's return NULL for unsupported types to stop processing.
        return NULL;
    }

    if (strlen(preproc_name) > 0) {
        preprocessor = find_plugin_by_name(preproc_name, pipeline->all_loaded_plugins, pipeline->num_all_plugins);
        if (!preprocessor) {
            LOG_ERROR("Preprocessor plugin '%s' not found.", preproc_name);
            return NULL;
        }
        LOG_INFO("Found preprocessor: %s (Version: %s, Type: %d)", preprocessor->info.name, preprocessor->info.version, preprocessor->info.type);
    }

    if (strlen(parser_name) > 0) {
        parser = find_plugin_by_name(parser_name, pipeline->all_loaded_plugins, pipeline->num_all_plugins);
         if (!parser) {
            LOG_ERROR("Parser plugin '%s' not found.", parser_name);
            return NULL;
        }
        LOG_INFO("Found parser: %s (Version: %s, Type: %d)", parser->info.name, parser->info.version, parser->info.type);
    }


    ProcessedDataChunk *original_chunk = read_file_to_chunk(file_path);
    if (!original_chunk) {
        LOG_ERROR("Failed to read file content from %s", file_path);
        return NULL;
    }

    ProcessedDataChunk *preprocessed_chunk = NULL;
    if (preprocessor && preprocessor->process_data) {
        LOG_INFO("Invoking preprocessor '%s' for file %s", preprocessor->info.name, file_path);
        preprocessed_chunk = preprocessor->process_data(original_chunk);
        if (!preprocessed_chunk) {
            LOG_ERROR("Preprocessor plugin '%s' failed to process data for %s.", preprocessor->info.name, file_path);
            free_processed_data_chunk(original_chunk);
            return NULL;
        }
        LOG_INFO("Preprocessor '%s' completed. Output chunk (type %d) length: %zu",
                 preprocessor->info.name, preprocessed_chunk->data_type, preprocessed_chunk->length);
        if (preprocessed_chunk != original_chunk) {
             free_processed_data_chunk(original_chunk);
        }
    } else {
        LOG_INFO("No preprocessor found or required for %s. Using original content.", file_path);
        preprocessed_chunk = original_chunk;
    }


    ProcessedDataChunk *parsed_chunk = NULL;
    if (parser && parser->process_data) {
        LOG_INFO("Invoking parser '%s'", parser->info.name);
        parsed_chunk = parser->process_data(preprocessed_chunk);
        if (!parsed_chunk) {
            LOG_ERROR("Parser plugin '%s' failed to process data.", parser->info.name);
            free_processed_data_chunk(preprocessed_chunk);
            return NULL;
        }
        LOG_INFO("Parser '%s' completed. Output chunk (type %d) length: %zu",
                 parser->info.name, parsed_chunk->data_type, parsed_chunk->length);
        if (parsed_chunk != preprocessed_chunk) {
            free_processed_data_chunk(preprocessed_chunk);
        }
    } else {
        LOG_ERROR("No parser plugin found for %s. Cannot complete processing.", file_path);
        free_processed_data_chunk(preprocessed_chunk);
        return NULL;
    }

    // At this point, parsed_chunk contains the final processed data.
    // The caller (test harness) is now responsible for freeing it.
    LOG_INFO("File %s successfully processed. Final data (type %d) length: %zu. Source ID: %s",
             file_path, parsed_chunk->data_type, parsed_chunk->length, parsed_chunk->source_id ? parsed_chunk->source_id : "N/A");

    LOG_DEBUG("Final data from parser (first 50 bytes): %.*s", 50, (char*)parsed_chunk->data);

    // DO NOT free_processed_data_chunk(parsed_chunk); here anymore.
    // Instead, pass it to the indexer if one is configured.

    if (pipeline->active_indexer_plugin && parsed_chunk) {
        LOG_INFO("Passing data from parser to active indexer: %s", pipeline->active_indexer_plugin->info.name);
        // The indexer's process_data will use its own internally managed data (static global for now)
        ProcessedDataChunk* indexing_status_chunk = pipeline->active_indexer_plugin->process_data(parsed_chunk);

        if (indexing_status_chunk) {
            // Indexers might return a status chunk, which should be freed by the pipeline.
            LOG_INFO("Indexer '%s' returned a status chunk (type %d, length %zu). Freeing it.",
                     pipeline->active_indexer_plugin->info.name,
                     indexing_status_chunk->data_type,
                     indexing_status_chunk->length);
            free_processed_data_chunk(indexing_status_chunk);
        } else {
            LOG_INFO("Indexer '%s' completed (returned NULL).", pipeline->active_indexer_plugin->info.name);
        }
        // The original parsed_chunk was consumed by the indexer (or should have been managed by it)
        // Our current indexer plugins typically don't modify the input chunk but return new or NULL.
        // The es_indexer returns NULL. If it took ownership, we wouldn't free parsed_chunk.
        // If it copied data, parsed_chunk still needs freeing here.
        // Current es_indexer doesn't take ownership, it just uses the data.
        // So, parsed_chunk should be freed after indexing.
        free_processed_data_chunk(parsed_chunk);
        LOG_INFO("Finished processing for file: %s. Data passed to indexer. Returning NULL from pipeline.", file_path);
        return NULL; // Data consumed by indexer, pipeline flow ends for this item.
    } else if (parsed_chunk) {
        // No active indexer, but we have a parsed chunk.
        // This behavior might need to be defined: does it get returned or discarded?
        // For now, let's return it, as per previous behavior if no indexer.
        LOG_INFO("Finished processing for file: %s. No active indexer. Returning final parsed chunk.", file_path);
        return parsed_chunk;
    } else {
        // No parsed_chunk (parser failed), already logged.
        LOG_INFO("Finished processing for file: %s. No parsed data to index or return.", file_path);
        return NULL;
    }
}

void pipeline_shutdown(SpindexIngestionPipeline *pipeline) {
    if (!pipeline) return;
    LOG_INFO("Ingestion pipeline shutting down.");

    // Shutdown active indexer plugin
    // IMPORTANT: This assumes the plugin_loader's unload_plugins will also call shutdown.
    // However, if the plugin has specific state managed via plugin_data (which we aim for),
    // the pipeline should explicitly shut down the plugins it initialized with specific data.
    // The current ElasticsearchIndexer uses a static global, so its shutdown (called by unload_plugins)
    // will handle its own static data. This active_indexer_plugin_data is currently NULL.
    if (pipeline->active_indexer_plugin) {
        LOG_INFO("Pipeline explicitly acknowledging shutdown for active indexer: %s (actual shutdown by plugin_loader)",
                 pipeline->active_indexer_plugin->info.name);
        // If plugin_data was used:
        // if (pipeline->active_indexer_plugin->shutdown) {
        //     pipeline->active_indexer_plugin->shutdown(pipeline->active_indexer_plugin_data);
        // }
        // pipeline->active_indexer_plugin_data = NULL; // Clear our stored data pointer
    }

    pipeline->all_loaded_plugins = NULL; // Dereference, not free
    pipeline->num_all_plugins = 0;
    pipeline->app_config = NULL;      // Dereference
    pipeline->active_indexer_plugin = NULL; // Dereference
}
