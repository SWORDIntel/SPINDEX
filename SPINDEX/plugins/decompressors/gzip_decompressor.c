#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h> // For gzip decompression

// Adjust path as necessary
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"

#define CHUNK_SIZE 16384 // 16KB buffer for zlib inflation

// --- Forward declarations for plugin functions ---
void gzip_get_info(PluginInfo *info);
int gzip_init();
void gzip_shutdown();
ProcessedDataChunk* gzip_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
// Standard C linkage should be sufficient for dlsym to find these.
// If a specific export macro is needed (like MINIZ_EXPORT), it would be defined in a common project header.
// For now, we assume default visibility is fine.

void spindex_plugin_get_info(PluginInfo *info) {
    gzip_get_info(info);
}

int spindex_plugin_init() {
    return gzip_init();
}

void spindex_plugin_shutdown() {
    gzip_shutdown();
}

ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return gzip_process_data(input_chunk);
}

// --- Plugin Implementation ---

void gzip_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "Gzip Decompressor", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, "1.0", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    // Using the official enum value now that data_types.h is updated.
    info->type = SPINDEX_PLUGIN_TYPE_DECOMPRESSOR;

    strncpy(info->description, "Decompresses Gzip compressed files.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int gzip_init() {
    fprintf(stderr, "Gzip Decompressor plugin initialized.\n");
    // zlib doesn't require global initialization for this use case.
    return 0; // Success
}

void gzip_shutdown() {
    fprintf(stderr, "Gzip Decompressor plugin shut down.\n");
    // zlib doesn't require global cleanup for this use case.
}

ProcessedDataChunk* gzip_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        fprintf(stderr, "gzip_process_data: Invalid input chunk.\n");
        return NULL;
    }

    int ret;
    z_stream strm;
    unsigned char out_buffer[CHUNK_SIZE];
    
    // Variables for managing the dynamically growing output buffer
    unsigned char *decompressed_data = NULL;
    size_t decompressed_capacity = 0;
    size_t decompressed_total_size = 0;

    // Initialize zlib stream
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    // Initialize for gzip decompression
    // windowBits = 15 (max window size) + 16 (for gzip detection)
    ret = inflateInit2(&strm, 15 + 16); 
    if (ret != Z_OK) {
        fprintf(stderr, "gzip_process_data: inflateInit2 failed with error code %d.\n", ret);
        return NULL;
    }

    strm.avail_in = input_chunk->length;
    strm.next_in = (unsigned char *)input_chunk->data;

    // Main decompression loop
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = out_buffer;
        ret = inflate(&strm, Z_NO_FLUSH);

        switch (ret) {
            case Z_NEED_DICT:
                fprintf(stderr, "gzip_process_data: inflate failed, Z_NEED_DICT error.\n");
                (void)inflateEnd(&strm);
                free(decompressed_data);
                return NULL;
            case Z_DATA_ERROR:
                fprintf(stderr, "gzip_process_data: inflate failed, Z_DATA_ERROR: Invalid or incomplete gzip data. Error msg: %s\n", strm.msg ? strm.msg : "(no message)");
                (void)inflateEnd(&strm);
                free(decompressed_data);
                return NULL;
            case Z_MEM_ERROR:
                fprintf(stderr, "gzip_process_data: inflate failed, Z_MEM_ERROR.\n");
                (void)inflateEnd(&strm);
                free(decompressed_data);
                return NULL;
            case Z_STREAM_ERROR:
                 fprintf(stderr, "gzip_process_data: inflate failed, Z_STREAM_ERROR (should not happen here).\n");
                (void)inflateEnd(&strm);
                free(decompressed_data);
                return NULL;

        }

        size_t have = CHUNK_SIZE - strm.avail_out;

        // Append data from out_buffer to decompressed_data
        if (decompressed_total_size + have > decompressed_capacity) {
            size_t new_capacity = (decompressed_total_size + have) * 2; // Or some other growth strategy
            if (new_capacity < CHUNK_SIZE) new_capacity = CHUNK_SIZE * 2; // Ensure growth
            if (new_capacity < decompressed_capacity + have) new_capacity = decompressed_capacity + have;


            unsigned char *new_data = (unsigned char *)realloc(decompressed_data, new_capacity);
            if (!new_data) {
                fprintf(stderr, "gzip_process_data: Failed to realloc memory for decompressed data.\n");
                (void)inflateEnd(&strm);
                free(decompressed_data);
                return NULL;
            }
            decompressed_data = new_data;
            decompressed_capacity = new_capacity;
        }
        memcpy(decompressed_data + decompressed_total_size, out_buffer, have);
        decompressed_total_size += have;

    } while (strm.avail_out == 0 && ret != Z_STREAM_END);

    // Clean up zlib stream
    (void)inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        fprintf(stderr, "gzip_process_data: Gzip stream not fully decompressed or an error occurred. Zlib ret: %d\n", ret);
        free(decompressed_data);
        return NULL;
    }

    // Create the output chunk
    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        fprintf(stderr, "gzip_process_data: Failed to allocate memory for output ProcessedDataChunk.\n");
        free(decompressed_data);
        return NULL;
    }

    // Shrink buffer to actual size if significantly oversized (optional)
    if (decompressed_total_size < decompressed_capacity / 2 && decompressed_total_size > 0) {
        unsigned char *final_data = (unsigned char *)realloc(decompressed_data, decompressed_total_size);
        if (final_data) {
            decompressed_data = final_data;
            decompressed_capacity = decompressed_total_size;
        } // If realloc fails, we still have the slightly larger buffer, which is fine.
    }


    output_chunk->data = decompressed_data;
    output_chunk->length = decompressed_total_size;
    output_chunk->data_type = CHUNK_TYPE_RAW_BYTES; // Or input_chunk->data_type
    output_chunk->source_id = NULL;

    if (input_chunk->source_id) {
        output_chunk->source_id = strdup(input_chunk->source_id);
        if (!output_chunk->source_id) {
            fprintf(stderr, "gzip_process_data: Failed to duplicate source_id (non-critical).\n");
        }
    }
    
    fprintf(stderr, "gzip_process_data: Successfully decompressed data into %zu bytes.\n", decompressed_total_size);

    // Memory management for input_chunk->data is assumed to be handled by the pipeline.
    return output_chunk;
}
