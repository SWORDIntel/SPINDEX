#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h> // For AVX-512 intrinsics (though not used in process_data yet)

// Adjust paths as necessary
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/hardware_detection.h" // For checking AVX-512 support
#include "../../include/avx_search_params.h"  // For AvxSearchParams
#include "../../include/utils.h" // For LOG_INFO, LOG_ERROR, LOG_DEBUG (if not already implicitly included)

// --- Forward declarations for plugin functions ---
void avx512_get_info(PluginInfo *info);
int avx512_init();
void avx512_shutdown();
ProcessedDataChunk* avx512_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
// Standard C linkage should be sufficient for dlsym to find these.

void spindex_plugin_get_info(PluginInfo *info) {
    avx512_get_info(info);
}

int spindex_plugin_init() {
    return avx512_init();
}

void spindex_plugin_shutdown() {
    avx512_shutdown();
}

ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return avx512_process_data(input_chunk);
}

// --- Plugin Implementation ---

void avx512_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "AVX-512 Phrase Searcher", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, "0.1", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    // SPINDEX_PLUGIN_TYPE_OTHER is used as a placeholder.
    // A more specific type like SPINDEX_PLUGIN_TYPE_SEARCH_ALGORITHM or
    // SPINDEX_PLUGIN_TYPE_ANALYZER could be defined later if the plugin system
    // is extended to support different kinds of processing functions beyond
    // the single input/output ProcessedDataChunk model.
    info->type = SPINDEX_PLUGIN_TYPE_OTHER; 

    strncpy(info->description, "Performs phrase searching using AVX-512 instructions.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int avx512_init() {
    CpuFeatures features;
    if (detect_cpu_features(&features) != 0) {
        fprintf(stderr, "AVX-512 Plugin: Failed to detect CPU features.\n");
        return -1; // Initialization failed: Cannot determine CPU capabilities
    }

    // Check for necessary AVX-512 support. 
    // AVX-512F (Foundation) is the base for most AVX-512.
    // AVX-512BW (Byte and Word) is often useful for text processing.
    if (!features.has_avx512f || !features.has_avx512bw) {
        fprintf(stderr, "AVX-512 Plugin: CPU does not support required AVX-512F and AVX-512BW features.\n");
        fprintf(stderr, "AVX-512F: %s, AVX-512BW: %s\n",
                features.has_avx512f ? "Supported" : "Not Supported",
                features.has_avx512bw ? "Supported" : "Not Supported");
        return -1; // Initialization failed: Required features not present
    }

    fprintf(stdout, "AVX-512 Phrase Searcher plugin initialized successfully (AVX-512F & AVX-512BW detected).\n");
    return 0; // Success
}

void avx512_shutdown() {
    fprintf(stdout, "AVX-512 Phrase Searcher plugin shutting down.\n");
    // No specific resources to release for this basic version.
}

// TODO: The plugin_interface.h currently only supports one input chunk for process_data.
// This plugin ideally needs two inputs: one for the text to search and one for the phrases.
// This will need to be addressed by either modifying the interface or by devising a way
// to pack both inputs into a single ProcessedDataChunk (e.g., using a custom data structure
// within the chunk's data field, or by having the plugin retrieve the phrases via a configuration mechanism).
ProcessedDataChunk* avx512_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data) {
        LOG_ERROR("AVX Search: Input chunk or its data is NULL.");
        return NULL;
    }
    const AvxSearchParams* params = (const AvxSearchParams*)input_chunk->data;
    if (!params || !params->text_to_search || !params->text_to_search->data ||
        !params->phrases_to_find || !params->phrases_to_find->data) {
        LOG_ERROR("AVX Search: Invalid search parameters, text, or phrases data.");
        return NULL;
    }

    const ProcessedDataChunk* text_chunk = params->text_to_search;
    const ProcessedDataChunk* phrases_chunk = params->phrases_to_find;

    LOG_INFO("AVX Search: Processing text of length %zu, with phrases of length %zu.", text_chunk->length, phrases_chunk->length);

    // --- This is a simplified way to get phrases. A real plugin might need more robust parsing ---
    char* phrases_str_copy = (char*)malloc(phrases_chunk->length + 1);
    if (!phrases_str_copy) {
        LOG_ERROR("AVX Search: Failed to allocate memory for phrases copy.");
        return NULL;
    }
    memcpy(phrases_str_copy, phrases_chunk->data, phrases_chunk->length);
    phrases_str_copy[phrases_chunk->length] = '\0';

    char* saveptr;
    char* current_phrase = strtok_r(phrases_str_copy, "\n", &saveptr);
    char result_buffer[1024] = {0}; 
    strncat(result_buffer, "Search Results:\n", sizeof(result_buffer) - 1);


    while (current_phrase != NULL) {
        LOG_DEBUG("AVX Search: Searching for phrase: '%s'", current_phrase);
        bool found_phrase = false;
        size_t phrase_len = strlen(current_phrase);

        if (phrase_len > 0 && phrase_len <= text_chunk->length) {
            // WARNING: The following AVX-512 code is a simplified demonstration for checking initial substring matches.
            // It is NOT a complete or robust AVX-512 phrase search algorithm. A production-quality implementation
            // would require a much more sophisticated approach (e.g., handling arbitrary lengths, advanced SIMD algorithms).
            
            size_t compare_len = phrase_len < 64 ? phrase_len : 64; 

            if (text_chunk->length >= compare_len) {
               __m512i phrase_vector = _mm512_loadu_si512((const __m512i*)current_phrase); 

               for (size_t i = 0; i <= text_chunk->length - compare_len; ++i) {
                   __m512i text_vector = _mm512_loadu_si512((const __m512i*)((const char*)text_chunk->data + i));
                   __mmask64 match_mask = _mm512_cmpeq_epi8_mask(phrase_vector, text_vector);
                   
                   uint64_t relevant_bits_mask = (compare_len == 64) ? 0xFFFFFFFFFFFFFFFFULL : (1ULL << compare_len) - 1;

                   if ((match_mask & relevant_bits_mask) == relevant_bits_mask) {
                       if (compare_len == phrase_len) {
                          found_phrase = true;
                          LOG_INFO("AVX Search: AVX-assisted match for initial part of '%s' at offset %zu", current_phrase, i);
                          break; 
                       } else {
                          LOG_DEBUG("AVX Search: Potential partial match for '%s' at offset %zu. Full verification needed.", current_phrase, i);
                          // For this demo, if the first part matches, assume it's found.
                          found_phrase = true; 
                          break;
                       }
                   }
               }
            }
        }

        if (found_phrase) {
            strncat(result_buffer, "Found: ", sizeof(result_buffer) - strlen(result_buffer) - 1);
            strncat(result_buffer, current_phrase, sizeof(result_buffer) - strlen(result_buffer) - 1);
        } else {
            strncat(result_buffer, "Not Found: ", sizeof(result_buffer) - strlen(result_buffer) - 1);
            strncat(result_buffer, current_phrase, sizeof(result_buffer) - strlen(result_buffer) - 1);
        }
        strncat(result_buffer, "\n", sizeof(result_buffer) - strlen(result_buffer) - 1);
        current_phrase = strtok_r(NULL, "\n", &saveptr);
    }
    free(phrases_str_copy);

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("AVX Search: Failed to allocate output chunk.");
        return NULL;
    }
    output_chunk->data = strdup(result_buffer); 
    if (!output_chunk->data) {
        LOG_ERROR("AVX Search: Failed to strdup result_buffer.");
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = strlen(result_buffer);
    output_chunk->source_id = text_chunk->source_id ? strdup(text_chunk->source_id) : NULL; 
    if (text_chunk->source_id && !output_chunk->source_id) {
         LOG_WARN("AVX Search: Failed to duplicate source_id for output chunk.");
    }
    output_chunk->data_type = CHUNK_TYPE_UTF8_TEXT; 

    LOG_INFO("AVX Search: Processing complete. Results:\n%s", result_buffer);
    return output_chunk;
}
