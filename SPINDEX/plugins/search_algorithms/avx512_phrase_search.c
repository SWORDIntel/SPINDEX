#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // For uint64_t
#include <immintrin.h> // For AVX-512 intrinsics (though not used in process_data yet)

// Adjust paths as necessary
#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/hardware_detection.h" // For checking AVX-512 support
#include "../../include/avx_search_params.h"  // For AvxSearchParams
#include "../../include/utils.h" // For LOG_INFO, LOG_ERROR, LOG_DEBUG (if not already implicitly included)
#include <stdbool.h>   // For bool type
#include <ctype.h>     // For tolower (optional, can use custom)

#define AVX_BLOCK_SIZE 64

// Helper function for lowercasing a character
static inline char tolower_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

// --- Forward declarations for plugin functions ---
void avx512_get_info(PluginInfo *info);
int avx512_init();
void avx512_shutdown();
ProcessedDataChunk* avx512_process_data(const ProcessedDataChunk *input_chunk);

// --- Exposed plugin entry points ---
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
    
    info->type = SPINDEX_PLUGIN_TYPE_OTHER; 

    strncpy(info->description, "Performs phrase searching using AVX-512 instructions.", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int avx512_init() {
    CpuFeatures features;
    if (detect_cpu_features(&features) != 0) {
        fprintf(stderr, "AVX-512 Plugin: Failed to detect CPU features.\n");
        return -1;
    }

    if (!features.has_avx512f || !features.has_avx512bw) {
        fprintf(stderr, "AVX-512 Plugin: CPU does not support required AVX-512F and AVX-512BW features.\n");
        fprintf(stderr, "AVX-512F: %s, AVX-512BW: %s\n",
                features.has_avx512f ? "Supported" : "Not Supported",
                features.has_avx512bw ? "Supported" : "Not Supported");
        return -1;
    }

    fprintf(stdout, "AVX-512 Phrase Searcher plugin initialized successfully (AVX-512F & AVX-512BW detected).\n");
    return 0;
}

void avx512_shutdown() {
    fprintf(stdout, "AVX-512 Phrase Searcher plugin shutting down.\n");
}

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

    bool case_sensitive = params->case_sensitive;
    bool find_all_occurrences = params->find_all_occurrences;
    int max_hits_per_doc = params->max_hits_per_doc;

    char* phrases_str_copy = (char*)malloc(phrases_chunk->length + 1);
    if (!phrases_str_copy) {
        LOG_ERROR("AVX Search: Failed to allocate memory for phrases copy.");
        return NULL;
    }
    memcpy(phrases_str_copy, phrases_chunk->data, phrases_chunk->length);
    phrases_str_copy[phrases_chunk->length] = '\0';

    size_t found_phrases_buffer_size = 1024;
    char* found_phrases_buffer = malloc(found_phrases_buffer_size);
    if (!found_phrases_buffer) {
        LOG_ERROR("AVX Search: Failed to allocate memory for found phrases buffer.");
        free(phrases_str_copy);
        return NULL;
    }
    found_phrases_buffer[0] = '\0';
    size_t found_phrases_len = 0;

    char* saveptr;
    char* current_phrase_str = strtok_r(phrases_str_copy, "\n", &saveptr);

    while (current_phrase_str != NULL) {
        size_t phrase_len = strlen(current_phrase_str);
        if (phrase_len == 0) {
            current_phrase_str = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        for (size_t i = 0; i <= text_chunk->length - phrase_len; ++i) {
            bool match = true;
            for (size_t j = 0; j < phrase_len; j += AVX_BLOCK_SIZE) {
                size_t chunk_len = (phrase_len - j < AVX_BLOCK_SIZE) ? (phrase_len - j) : AVX_BLOCK_SIZE;
                __mmask64 mask = (1ULL << chunk_len) - 1;

                __m512i phrase_vec = _mm512_maskz_loadu_epi8(mask, current_phrase_str + j);
                __m512i text_vec = _mm512_maskz_loadu_epi8(mask, (char*)text_chunk->data + i + j);

                if (!case_sensitive) {
                    // This is a simplified lowercase, for ASCII only.
                    __m512i a_vec = _mm512_set1_epi8('a');
                    __m512i z_vec = _mm512_set1_epi8('z');
                    __m512i A_vec = _mm512_set1_epi8('A');
                    __m512i Z_vec = _mm512_set1_epi8('Z');
                    __m512i thirty_two = _mm512_set1_epi8(32);

                    __mmask64 phrase_upper_mask = _mm512_cmplt_epi8_mask(phrase_vec, Z_vec) & _mm512_cmpge_epi8_mask(phrase_vec, A_vec);
                    phrase_vec = _mm512_mask_add_epi8(phrase_vec, phrase_upper_mask, phrase_vec, thirty_two);

                    __mmask64 text_upper_mask = _mm512_cmplt_epi8_mask(text_vec, Z_vec) & _mm512_cmpge_epi8_mask(text_vec, A_vec);
                    text_vec = _mm512_mask_add_epi8(text_vec, text_upper_mask, text_vec, thirty_two);
                }

                if (_mm512_mask_cmpeq_epi8_mask(mask, phrase_vec, text_vec) != mask) {
                    match = false;
                    break;
                }
            }

            if (match) {
                if (found_phrases_len + phrase_len + 1 > found_phrases_buffer_size) {
                    found_phrases_buffer_size *= 2;
                    char* new_buffer = realloc(found_phrases_buffer, found_phrases_buffer_size);
                    if (!new_buffer) {
                        LOG_ERROR("AVX Search: Failed to reallocate memory for found phrases buffer.");
                        free(found_phrases_buffer);
                        free(phrases_str_copy);
                        return NULL;
                    }
                    found_phrases_buffer = new_buffer;
                }
                strcat(found_phrases_buffer, current_phrase_str);
                strcat(found_phrases_buffer, "\n");
                found_phrases_len += phrase_len + 1;

                if (!find_all_occurrences) {
                    goto next_phrase;
                }
            }
        }
    next_phrase:
        current_phrase_str = strtok_r(NULL, "\n", &saveptr);
    }

    free(phrases_str_copy);

    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("AVX Search: Failed to allocate output chunk.");
        free(found_phrases_buffer);
        return NULL;
    }
    output_chunk->data = (unsigned char*)found_phrases_buffer;
    output_chunk->length = found_phrases_len;
    output_chunk->source_id = text_chunk->source_id ? strdup(text_chunk->source_id) : NULL;
    output_chunk->data_type = CHUNK_TYPE_UTF8_TEXT;

    return output_chunk;
}
