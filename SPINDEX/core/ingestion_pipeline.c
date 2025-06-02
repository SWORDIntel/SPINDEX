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

    LOG_INFO("Ingestion pipeline initialized. Aware of %d loaded plugins.", num_plugins);
    LOG_INFO("Plugin directory from config: %s", config->plugin_directory); // Example: using config

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
        strcpy(parser_name, "JSONParser");   // Now using JSONParser
        LOG_DEBUG("File type JSON identified. Required preprocessor: '%s', parser: '%s'", preproc_name, parser_name);
    } else if (strcmp(extension, "txt") == 0) {
        // strcpy(preproc_name, "TxtPreprocessor");
        // strcpy(parser_name, "TxtParser");
        LOG_WARN("Plain text file (.txt) processing not fully defined yet. Using DummyPlugin as placeholder.", extension);
        strcpy(preproc_name, "DummyPlugin"); // Placeholder
        strcpy(parser_name, "DummyPlugin");   // Placeholder
    } else {
        LOG_ERROR("Unsupported file extension: .%s for file %s", extension, file_path);
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
    LOG_INFO("Finished processing for file: %s. Returning final chunk.", file_path);
    return parsed_chunk; // Return the final chunk
}

void pipeline_shutdown(SpindexIngestionPipeline *pipeline) {
    if (!pipeline) return;
    LOG_INFO("Ingestion pipeline shutting down.");
    // Free any resources specifically allocated by the pipeline struct itself, if any.
    // For now, the struct mostly holds references, so not much to do here.
    pipeline->all_loaded_plugins = NULL;
    pipeline->num_all_plugins = 0;
    pipeline->app_config = NULL;
}
