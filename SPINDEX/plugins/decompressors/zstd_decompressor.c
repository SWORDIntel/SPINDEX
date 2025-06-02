#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h> // For Zstandard decompression

// Adjust path as necessary
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"

// --- Forward declarations for plugin functions ---
void zstd_get_info(PluginInfo *info);
int zstd_init();
void zstd_shutdown();
ProcessedDataChunk* zstd_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
// Standard C linkage should be sufficient for dlsym to find these.

void spindex_plugin_get_info(PluginInfo *info) {
    zstd_get_info(info);
}

int spindex_plugin_init() {
    return zstd_init();
}

void spindex_plugin_shutdown() {
    zstd_shutdown();
}

ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return zstd_process_data(input_chunk);
}

// --- Plugin Implementation ---

void zstd_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "Zstandard Decompressor", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, "1.0", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    // Using the official enum value now that data_types.h is updated.
    info->type = SPINDEX_PLUGIN_TYPE_DECOMPRESSOR;

    strncpy(info->description, "Decompresses Zstandard compressed files.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int zstd_init() {
    fprintf(stderr, "Zstandard Decompressor plugin initialized.\n");
    // libzstd doesn't require global initialization for this use case.
    return 0; // Success
}

void zstd_shutdown() {
    fprintf(stderr, "Zstandard Decompressor plugin shut down.\n");
    // libzstd doesn't require global cleanup for this use case.
}

ProcessedDataChunk* zstd_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        fprintf(stderr, "zstd_process_data: Invalid input chunk.\n");
        return NULL;
    }

    // Get the decompressed size
    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(input_chunk->data, input_chunk->length);

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        fprintf(stderr, "zstd_process_data: ZSTD_getFrameContentSize reported an error (not a ZSTD frame or corrupted?).\n");
        return NULL;
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        // For this version, we require the content size to be known for one-shot decompression.
        // A future version could implement streaming decompression.
        fprintf(stderr, "zstd_process_data: ZSTD_getFrameContentSize returned ZSTD_CONTENTSIZE_UNKNOWN. Streaming decompression not yet implemented.\n");
        return NULL;
    }
    if (decompressed_size == 0) { // Valid empty frame
         fprintf(stderr, "zstd_process_data: Input is an empty ZSTD frame.\n");
         ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)calloc(1, sizeof(ProcessedDataChunk)); // Use calloc for zero-init
         if(!output_chunk) {
            fprintf(stderr, "zstd_process_data: Failed to allocate memory for empty output ProcessedDataChunk.\n");
            return NULL;
         }
         // data is NULL, length is 0, type can be RAW_BYTES or copied.
         output_chunk->data_type = CHUNK_TYPE_RAW_BYTES;
         if (input_chunk->source_id) {
            output_chunk->source_id = strdup(input_chunk->source_id);
         }
         return output_chunk;
    }


    // Allocate memory for the decompressed data
    void *output_buffer = malloc(decompressed_size);
    if (!output_buffer) {
        fprintf(stderr, "zstd_process_data: Failed to allocate %llu bytes for decompressed data.\n", decompressed_size);
        return NULL;
    }

    // Decompress
    size_t const actual_decompressed_size = ZSTD_decompress(output_buffer, decompressed_size, input_chunk->data, input_chunk->length);

    if (ZSTD_isError(actual_decompressed_size)) {
        fprintf(stderr, "zstd_process_data: ZSTD_decompress failed: %s\n", ZSTD_getErrorName(actual_decompressed_size));
        free(output_buffer);
        return NULL;
    }

    if (actual_decompressed_size != decompressed_size) {
        // This case should ideally not happen if ZSTD_getFrameContentSize was accurate and no error occurred.
        fprintf(stderr, "zstd_process_data: Warning - actual decompressed size %zu differs from expected %llu.\n",
                actual_decompressed_size, decompressed_size);
        // We might choose to proceed with actual_decompressed_size or error out.
        // For robustness, if it's smaller, we could realloc, but if larger, it's an issue.
        // Given ZSTD_decompress's contract, this implies an issue. Let's treat as error for now or trust actual_decompressed_size.
        // For simplicity, if ZSTD_decompress didn't error, we trust actual_decompressed_size.
        // However, a discrepancy usually means ZSTD_getFrameContentSize was not appropriate for ZSTD_decompress (e.g. multiple frames)
        // or there's some other subtle issue.
        // Given we checked ZSTD_isError, actual_decompressed_size should be correct if no error.
    }

    // Create the output chunk
    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        fprintf(stderr, "zstd_process_data: Failed to allocate memory for output ProcessedDataChunk.\n");
        free(output_buffer);
        return NULL;
    }

    output_chunk->data = output_buffer;
    output_chunk->length = actual_decompressed_size; // Use the size returned by ZSTD_decompress
    output_chunk->data_type = CHUNK_TYPE_RAW_BYTES; // Or input_chunk->data_type
    output_chunk->source_id = NULL;

    if (input_chunk->source_id) {
        output_chunk->source_id = strdup(input_chunk->source_id);
        if (!output_chunk->source_id) {
            fprintf(stderr, "zstd_process_data: Failed to duplicate source_id (non-critical).\n");
        }
    }
    
    fprintf(stderr, "zstd_process_data: Successfully decompressed data into %zu bytes.\n", actual_decompressed_size);

    // Memory management for input_chunk->data is assumed to be handled by the pipeline.
    return output_chunk;
}
