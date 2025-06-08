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

// Helper function to escape a string for JSON output
static size_t escape_json_string(const char *input, char *output, size_t out_size) {
    if (!input || !output || out_size == 0) {
        if (output && out_size > 0) output[0] = '\0';
        return 0;
    }

    size_t written = 0;
    const char *p = input;
    char *q = output;

    while (*p && written < out_size - 1) { // -1 for null terminator
        switch (*p) {
            case '\\':
            case '"':
                if (written + 2 > out_size - 1) goto end; // Need space for '\' and char
                *q++ = '\\';
                *q++ = *p;
                written += 2;
                break;
            case '\n':
                if (written + 2 > out_size - 1) goto end;
                *q++ = '\\';
                *q++ = 'n';
                written += 2;
                break;
            case '\r':
                if (written + 2 > out_size - 1) goto end;
                *q++ = '\\';
                *q++ = 'r';
                written += 2;
                break;
            case '\t':
                if (written + 2 > out_size - 1) goto end;
                *q++ = '\\';
                *q++ = 't';
                written += 2;
                break;
            case '\b': // Backspace
                if (written + 2 > out_size - 1) goto end;
                *q++ = '\\';
                *q++ = 'b';
                written += 2;
                break;
            case '\f': // Form feed
                if (written + 2 > out_size - 1) goto end;
                *q++ = '\\';
                *q++ = 'f';
                written += 2;
                break;
            default:
                // For other control characters (0x00-0x1F), you might want to use \uXXXX
                // but for this implementation, we'll pass them through if printable,
                // or skip if not (though most common ones are handled above).
                // This basic version focuses on the most common JSON escapes.
                if (*p >= 0x20 && *p <= 0x7E) { // Printable ASCII
                    *q++ = *p;
                    written++;
                } else {
                    // Potentially skip or replace non-printable characters not explicitly handled
                    // For simplicity, we'll skip them here.
                    // A more robust solution might use \uXXXX escaping.
                }
                break;
        }
        p++;
    }

end:
    if (written < out_size) {
        *q = '\0'; // Null-terminate the output string
    } else if (out_size > 0) {
        output[out_size - 1] = '\0'; // Ensure null termination if buffer is full
    }
    return written;
}


#define RESULT_BUFFER_SIZE 8192 // Increased buffer size for JSON output
#define AVX_BLOCK_SIZE 64  // AVX-512 processes 64 bytes at a time (already defined in example, ensure consistency if it was different)

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

    // Read parameters from AvxSearchParams
    bool case_sensitive = params->case_sensitive;
    bool find_all_occurrences = params->find_all_occurrences;

    LOG_INFO("AVX Search: Processing text of length %zu, with phrases of length %zu. Case sensitive: %s, Find all: %s",
             text_chunk->length, phrases_chunk->length, case_sensitive ? "true" : "false", find_all_occurrences ? "true" : "false");

    // --- This is a simplified way to get phrases. A real plugin might need more robust parsing ---
    char* phrases_str_copy = (char*)malloc(phrases_chunk->length + 1);
    if (!phrases_str_copy) {
        LOG_ERROR("AVX Search: Failed to allocate memory for phrases copy.");
        return NULL;
    }
    memcpy(phrases_str_copy, phrases_chunk->data, phrases_chunk->length);
    phrases_str_copy[phrases_chunk->length] = '\0';

    char* saveptr;
    char* current_phrase_str = strtok_r(phrases_str_copy, "\n", &saveptr);

    char result_buffer[RESULT_BUFFER_SIZE] = {0};
    int current_buffer_pos = 0;
    current_buffer_pos += snprintf(result_buffer, RESULT_BUFFER_SIZE, "{\"search_results\": [");
    bool first_phrase_in_json = true;

    while (current_phrase_str != NULL) {
        LOG_DEBUG("AVX Search: Searching for phrase: '%s'", current_phrase_str);
        size_t phrase_len = strlen(current_phrase_str);
        bool phrase_had_occurrences = false;

        if (!first_phrase_in_json) {
            // Add comma if not the first phrase object
            if (current_buffer_pos < RESULT_BUFFER_SIZE - 1) {
                result_buffer[current_buffer_pos++] = ',';
            } else { LOG_WARN("AVX Search: Buffer full before adding comma for phrase object."); }
        }
        first_phrase_in_json = false;

        // Start phrase object: {"phrase": "...", "offsets": [
        // Proper JSON escaping for current_phrase_str
        char escaped_phrase_buffer[2 * strlen(current_phrase_str) + 1]; // Max 2 chars per input char + null
        escape_json_string(current_phrase_str, escaped_phrase_buffer, sizeof(escaped_phrase_buffer));

        int pre_phrase_len = current_buffer_pos;
        current_buffer_pos += snprintf(result_buffer + current_buffer_pos, RESULT_BUFFER_SIZE - current_buffer_pos,
                                       "{\"phrase\": \"%s\", \"offsets\": [", escaped_phrase_buffer);
        if (current_buffer_pos >= RESULT_BUFFER_SIZE -1) {
             LOG_WARN("AVX Search: Result buffer truncated while writing phrase header.");
             result_buffer[pre_phrase_len] = '\0'; // Attempt to backtrack to keep JSON valid-ish
             current_buffer_pos = pre_phrase_len;
             goto finalize_json; // Early exit if buffer is full
        }
        bool first_offset_for_this_phrase = true;

        if (phrase_len == 0) {
            LOG_DEBUG("AVX Search: Skipping empty phrase.");
        } else if (phrase_len > text_chunk->length) {
            LOG_DEBUG("AVX Search: Phrase '%s' (len %zu) is longer than text (len %zu).", current_phrase, phrase_len, text_chunk->length);
        } else {
            for (size_t i = 0; i <= text_chunk->length - phrase_len; ++i) {
                bool current_match_is_full = true;
                for (size_t j = 0; j < phrase_len; j += AVX_BLOCK_SIZE) {
                    size_t bytes_to_compare_this_iteration = (phrase_len - j < AVX_BLOCK_SIZE) ? (phrase_len - j) : AVX_BLOCK_SIZE;
                    __mmask64 load_compare_mask = (bytes_to_compare_this_iteration == AVX_BLOCK_SIZE) ? 0xFFFFFFFFFFFFFFFFULL : (1ULL << bytes_to_compare_this_iteration) - 1;

                    __m512i phrase_vector;
                    __m512i text_vector;

                    if (!case_sensitive) {
                        char temp_phrase_segment[AVX_BLOCK_SIZE];
                        char temp_text_segment[AVX_BLOCK_SIZE];
                        for (size_t seg_idx = 0; seg_idx < bytes_to_compare_this_iteration; ++seg_idx) {
                            temp_phrase_segment[seg_idx] = tolower_char(current_phrase_str[j + seg_idx]);
                            temp_text_segment[seg_idx] = tolower_char(((const char*)text_chunk->data)[i + j + seg_idx]);
                        }
                        phrase_vector = _mm512_maskz_loadu_epi8(load_compare_mask, temp_phrase_segment);
                        text_vector = _mm512_maskz_loadu_epi8(load_compare_mask, temp_text_segment);
                    } else {
                        phrase_vector = _mm512_maskz_loadu_epi8(load_compare_mask, (const void*)(current_phrase_str + j));
                        text_vector = _mm512_maskz_loadu_epi8(load_compare_mask, (const void*)((const char*)text_chunk->data + i + j));
                    }

                    __mmask64 VEC_match_mask = _mm512_cmpeq_epi8_mask(phrase_vector, text_vector);
                    if ((VEC_match_mask & load_compare_mask) != load_compare_mask) {
                        current_match_is_full = false;
                        break;
                    }
                }

                if (current_match_is_full) {
                    phrase_had_occurrences = true;
                    if (!first_offset_for_this_phrase) {
                        if (current_buffer_pos < RESULT_BUFFER_SIZE - 1) {
                            result_buffer[current_buffer_pos++] = ',';
                        } else { LOG_WARN("AVX Search: Buffer full before adding comma for offset."); goto finalize_json_phrase; }
                    }
                    first_offset_for_this_phrase = false;

                    int pre_offset_len = current_buffer_pos;
                    current_buffer_pos += snprintf(result_buffer + current_buffer_pos, RESULT_BUFFER_SIZE - current_buffer_pos, "%zu", i);
                    if (current_buffer_pos >= RESULT_BUFFER_SIZE -1) {
                        LOG_WARN("AVX Search: Result buffer truncated while writing offset.");
                        result_buffer[pre_offset_len] = '\0'; // backtrack
                        current_buffer_pos = pre_offset_len;
                        goto finalize_json_phrase; // Early exit for this phrase's offsets
                    }
                    LOG_INFO("AVX Search: Found phrase '%s' at offset %zu (case_sensitive: %s)", current_phrase_str, i, case_sensitive ? "true" : "false");

                    if (!find_all_occurrences) {
                        break;
                    }
                }
            }
        }

finalize_json_phrase:
        // Close offsets array and phrase object: ]}
        if (current_buffer_pos < RESULT_BUFFER_SIZE - 2) {
            result_buffer[current_buffer_pos++] = ']';
            result_buffer[current_buffer_pos++] = '}';
        } else {
             LOG_WARN("AVX Search: Buffer full before closing phrase object.");
             if (current_buffer_pos < RESULT_BUFFER_SIZE -1) result_buffer[current_buffer_pos++] = ']'; // try to close array at least
             if (current_buffer_pos < RESULT_BUFFER_SIZE -1) result_buffer[current_buffer_pos++] = '}';
             result_buffer[RESULT_BUFFER_SIZE - 1] = '\0'; // Ensure null termination
             current_buffer_pos = RESULT_BUFFER_SIZE - 1;
             goto finalize_json; // Critical buffer full, finalize JSON now
        }
        current_phrase_str = strtok_r(NULL, "\n", &saveptr);
    }
    free(phrases_str_copy);

finalize_json:
    // Close JSON array
    if (current_buffer_pos < RESULT_BUFFER_SIZE - 2) {
        current_buffer_pos += snprintf(result_buffer + current_buffer_pos, RESULT_BUFFER_SIZE - current_buffer_pos, "]}");
    } else {
        // Buffer potentially full, ensure null termination if we were right at the edge
        result_buffer[RESULT_BUFFER_SIZE - 1] = '\0';
        current_buffer_pos = RESULT_BUFFER_SIZE - 1;
        LOG_WARN("AVX Search: Result buffer truncated while finalizing JSON.");
    }


    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        LOG_ERROR("AVX Search: Failed to allocate output chunk.");
        return NULL;
    }
    output_chunk->data = strdup(result_buffer); // strdup allocates new memory
    if (!output_chunk->data) {
        LOG_ERROR("AVX Search: Failed to strdup result_buffer.");
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = current_buffer_pos; // Use current_buffer_pos for accurate length
    output_chunk->source_id = text_chunk->source_id ? strdup(text_chunk->source_id) : NULL; 
    if (text_chunk->source_id && !output_chunk->source_id) {
         LOG_WARN("AVX Search: Failed to duplicate source_id for output chunk.");
    }
    output_chunk->data_type = CHUNK_TYPE_UTF8_TEXT; 

    LOG_INFO("AVX Search: Processing complete. Results:\n%s", result_buffer);
    return output_chunk;
}
