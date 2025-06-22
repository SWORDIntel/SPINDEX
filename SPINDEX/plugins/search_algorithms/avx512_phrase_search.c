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
            default:
                // For characters in the range 0x00 to 0x1F (ASCII control characters)
                // or 0x7F (DEL)
                if ((*p >= 0x00 && *p <= 0x1F) || *p == 0x7F) {
                    if (written + 6 > out_size - 1) goto end; // Need space for \uXXXX
                    // Use (unsigned char)*p to ensure correct hex formatting for values > 0x7F if char is signed
                    sprintf(q, "\\u%04X", (unsigned char)*p);
                    q += 6;
                    written += 6;
                }
                // For printable ASCII characters (excluding DEL, which is handled above)
                else if (*p >= 0x20 && *p < 0x7F) {
                    if (written + 1 > out_size - 1) goto end;
                    *q++ = *p;
                    written++;
                }
                // For characters >= 0x80 (e.g., part of a multi-byte UTF-8 sequence)
                // JSON spec allows these to be passed through if they are printable
                // or escaped with \uXXXX. For simplicity, and assuming valid UTF-8 input
                // (as per CHUNK_TYPE_UTF8_TEXT), we pass them through.
                // A more robust UTF-8 aware escaper would be significantly more complex.
                else {
                    // For valid multi-byte UTF-8 characters (>=0x80), pass them through directly.
                    // Assumes input is valid UTF-8 as per CHUNK_TYPE_UTF8_TEXT.
                    // A full UTF-8 validating/escaping routine would be more complex.
                    if (written + 1 > out_size - 1) goto end; // Should be based on UTF-8 char width if not passing raw
                    *q++ = *p; // Pass through
                    written++;
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

// This plugin receives its parameters (including the text to search and the phrases to find)
// via the input_chunk->data field, which is cast to an AvxSearchParams structure.
// This structure effectively packs multiple logical inputs into the single ProcessedDataChunk
// that the current plugin_interface.h supports for its process_data function.
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
    int max_hits_per_doc = params->max_hits_per_doc; // Added

    LOG_INFO("AVX Search: Processing text of length %zu, with phrases of length %zu. Case sensitive: %s, Find all: %s, Max hits/doc: %d",
             text_chunk->length, phrases_chunk->length, case_sensitive ? "true" : "false", find_all_occurrences ? "true" : "false", max_hits_per_doc); // Added max_hits_per_doc

    // --- This is a simplified way to get phrases. A real plugin might need more robust parsing ---
    char* phrases_str_copy = (char*)malloc(phrases_chunk->length + 1);
    if (!phrases_str_copy) {
        LOG_ERROR("AVX Search: Failed to allocate memory for phrases copy.");
        return NULL;
    }
    memcpy(phrases_str_copy, phrases_chunk->data, phrases_chunk->length);
    phrases_str_copy[phrases_chunk->length] = '\0';

    char* saveptr;
    // Phrases are parsed from a newline-separated string.
    // Each token extracted by strtok_r is considered a phrase to search for.
    char* current_phrase_str = strtok_r(phrases_str_copy, "\n", &saveptr);

    char result_buffer[RESULT_BUFFER_SIZE] = {0};
    int current_buffer_pos = 0;
    // Initial JSON structure: {"search_results": [
    // Start building the JSON output string.
    int required_len_initial = snprintf(NULL, 0, "{\"search_results\": [");
    if (required_len_initial + 1 >= RESULT_BUFFER_SIZE) {
        LOG_ERROR("AVX Search: Result buffer too small for even initial JSON structure. Size: %d, Required: %d", RESULT_BUFFER_SIZE, required_len_initial +1);
        // Cannot proceed, buffer is too small for anything meaningful
        free(phrases_str_copy); // phrases_str_copy was allocated before this
        return NULL; // Or return an error chunk
    }
    // Start search_results array
    current_buffer_pos += snprintf(result_buffer, RESULT_BUFFER_SIZE, "{\"search_results\": [");
    bool first_phrase_in_json = true;

    // Loop through each phrase obtained from the input phrases_chunk.
    while (current_phrase_str != NULL) {
        LOG_DEBUG("AVX Search: Searching for phrase: '%s'", current_phrase_str);
        size_t phrase_len = strlen(current_phrase_str);
        bool phrase_had_occurrences = false;
        int hits_for_current_phrase = 0; // Added

        if (!first_phrase_in_json) {
            // Add comma if not the first phrase object
            int required_len_comma = 1;
            if (current_buffer_pos + required_len_comma + 1 >= RESULT_BUFFER_SIZE) { // +1 for comma, +1 for null terminator
                LOG_WARN("AVX Search: Buffer full before adding comma for phrase object. Pos: %d", current_buffer_pos);
                // Buffer nearly full, attempt to finalize JSON and exit.
                goto finalize_json; // Not enough space for comma and more content
            }
            result_buffer[current_buffer_pos++] = ',';
        }
        first_phrase_in_json = false;

        // Start phrase object: {"phrase": "...", "offsets": [
        // Proper JSON escaping for current_phrase_str.
        size_t current_phrase_len_for_escape = strlen(current_phrase_str);
        char escaped_phrase_buffer[current_phrase_len_for_escape * 6 + 1]; // Max 6 chars per input char for \uXXXX + null
        escape_json_string(current_phrase_str, escaped_phrase_buffer, sizeof(escaped_phrase_buffer));

        int required_len_phrase_header = snprintf(NULL, 0, "{\"phrase\": \"%s\", \"offsets\": [", escaped_phrase_buffer);
        if (current_buffer_pos + required_len_phrase_header + 1 >= RESULT_BUFFER_SIZE) {
            LOG_WARN("AVX Search: Buffer full before writing phrase object header for '%s'. Pos: %d, Req: %d", current_phrase_str, current_buffer_pos, required_len_phrase_header);
            // Buffer nearly full, attempt to finalize JSON and exit.
            goto finalize_json;
        }
        // Add the phrase itself and start its offsets array.
        current_buffer_pos += snprintf(result_buffer + current_buffer_pos, RESULT_BUFFER_SIZE - current_buffer_pos,
                                       "{\"phrase\": \"%s\", \"offsets\": [", escaped_phrase_buffer);
        bool first_offset_for_this_phrase = true;

        if (phrase_len == 0) {
            LOG_DEBUG("AVX Search: Skipping empty phrase.");
        } else if (phrase_len > text_chunk->length) {
            LOG_DEBUG("AVX Search: Phrase '%s' (len %zu) is longer than text (len %zu).", current_phrase, phrase_len, text_chunk->length);
        } else {
            for (size_t i = 0; i <= text_chunk->length - phrase_len; ++i) {
                bool current_match_is_full = true;
                // Iterate through the phrase comparing it block by block (AVX_BLOCK_SIZE) against the text.
                for (size_t j = 0; j < phrase_len; j += AVX_BLOCK_SIZE) {
                    // Determine how many bytes to compare in this iteration (handles phrases not a multiple of block size).
                    size_t bytes_to_compare_this_iteration = (phrase_len - j < AVX_BLOCK_SIZE) ? (phrase_len - j) : AVX_BLOCK_SIZE;
                    // Create a mask for loading and comparing: only the relevant bytes in the vector are considered.
                    __mmask64 load_compare_mask = (bytes_to_compare_this_iteration == AVX_BLOCK_SIZE) ? 0xFFFFFFFFFFFFFFFFULL : (1ULL << bytes_to_compare_this_iteration) - 1;

                    __m512i phrase_vector;
                    __m512i text_vector;

                    if (!case_sensitive) {
                        // For case-insensitive search, lowercase temporary segments before loading into AVX registers.
                        char temp_phrase_segment[AVX_BLOCK_SIZE];
                        char temp_text_segment[AVX_BLOCK_SIZE];
                        for (size_t seg_idx = 0; seg_idx < bytes_to_compare_this_iteration; ++seg_idx) {
                            temp_phrase_segment[seg_idx] = tolower_char(current_phrase_str[j + seg_idx]);
                            temp_text_segment[seg_idx] = tolower_char(((const char*)text_chunk->data)[i + j + seg_idx]);
                        }
                        // Load the lowercased phrase segment into an AVX-512 register, masked by load_compare_mask.
                        phrase_vector = _mm512_maskz_loadu_epi8(load_compare_mask, temp_phrase_segment);
                        // Load the lowercased text segment into an AVX-512 register, masked by load_compare_mask.
                        text_vector = _mm512_maskz_loadu_epi8(load_compare_mask, temp_text_segment);
                    } else {
                        // Case-sensitive: Load phrase segment directly into an AVX-512 register.
                        phrase_vector = _mm512_maskz_loadu_epi8(load_compare_mask, (const void*)(current_phrase_str + j));
                        // Case-sensitive: Load text segment directly into an AVX-512 register.
                        text_vector = _mm512_maskz_loadu_epi8(load_compare_mask, (const void*)((const char*)text_chunk->data + i + j));
                    }

                    // Perform a vectorized byte-wise comparison between phrase_vector and text_vector.
                    // Result is a mask where each bit corresponds to a byte comparison result.
                    __mmask64 VEC_match_mask = _mm512_cmpeq_epi8_mask(phrase_vector, text_vector);

                    // Check if all relevant bytes in the current segment matched.
                    // This is done by ANDing the comparison result mask with the load_compare_mask
                    // (to ignore irrelevant bytes at the end of the vector) and checking if it's still equal to load_compare_mask.
                    if ((VEC_match_mask & load_compare_mask) != load_compare_mask) {
                        current_match_is_full = false; // Mismatch found in this segment.
                        break; // Exit inner loop (j), this text position (i) is not a match.
                    }
                }

                if (current_match_is_full) {
                    phrase_had_occurrences = true;
                    if (!first_offset_for_this_phrase) {
                        int required_len_comma_offset = 1;
                        if (current_buffer_pos + required_len_comma_offset + 1 >= RESULT_BUFFER_SIZE) {
                            LOG_WARN("AVX Search: Buffer full before adding comma for offset for phrase '%s'. Pos: %d", current_phrase_str, current_buffer_pos);
                            // Buffer nearly full, attempt to finalize JSON and exit.
                            goto finalize_json_phrase;
                        }
                        result_buffer[current_buffer_pos++] = ',';
                    }
                    first_offset_for_this_phrase = false;

                    // Add offset to array
                    int required_len_offset = snprintf(NULL, 0, "%zu", i);
                    if (current_buffer_pos + required_len_offset + 1 >= RESULT_BUFFER_SIZE) {
                        LOG_WARN("AVX Search: Buffer full before writing offset for phrase '%s'. Pos: %d, Req: %d", current_phrase_str, current_buffer_pos, required_len_offset);
                        // Buffer nearly full, attempt to finalize JSON and exit.
                        goto finalize_json_phrase;
                    }
                    current_buffer_pos += snprintf(result_buffer + current_buffer_pos, RESULT_BUFFER_SIZE - current_buffer_pos, "%zu", i);
                    LOG_INFO("AVX Search: Found phrase '%s' at offset %zu (case_sensitive: %s)", current_phrase_str, i, case_sensitive ? "true" : "false");

                    hits_for_current_phrase++; // Added
                    if (find_all_occurrences && max_hits_per_doc > 0 && hits_for_current_phrase >= max_hits_per_doc) { // Added block
                        LOG_DEBUG("AVX Search: Reached max_hits_per_doc (%d) for phrase '%s'.", max_hits_per_doc, current_phrase_str);
                        break; // Break from inner loop (finding occurrences of current phrase)
                    }

                    if (!find_all_occurrences) {
                        break;
                    }
                }
            }
        }

finalize_json_phrase:
        // Close offsets array and phrase object: ]}
        int required_len_close_phrase = 2; // For "]}"
        if (current_buffer_pos + required_len_close_phrase + 1 >= RESULT_BUFFER_SIZE) {
            LOG_WARN("AVX Search: Buffer full before closing phrase object for '%s'. Pos: %d, Req: %d", current_phrase_str, current_buffer_pos, required_len_close_phrase);
            // Buffer nearly full, attempt to finalize JSON and exit.
            goto finalize_json; // Go to overall finalize, as this phrase object is problematic
        }
        // Close phrase object (offsets array and the object itself)
        result_buffer[current_buffer_pos++] = ']'; // Close offsets array
        result_buffer[current_buffer_pos++] = '}'; // Close phrase object
        current_phrase_str = strtok_r(NULL, "\n", &saveptr);
    }
    free(phrases_str_copy);

finalize_json: // Label for jumping to when the buffer is full or processing is done.
    // Finalize JSON output: Close the main search_results array.
    int required_len_finalize = 2; // For "]}"
    bool is_empty_json = (strcmp(result_buffer, "{\"search_results\": [") == 0);

    if (is_empty_json) { // If result_buffer is just "{\"search_results\": ["
        // Make it "{\"search_results\": []}"
        required_len_finalize = 1; // Just need "]"
         if (current_buffer_pos + required_len_finalize + 1 > RESULT_BUFFER_SIZE) { // +1 for '}' and +1 for null
            LOG_WARN("AVX Search: Result buffer too small to close empty JSON array. Pos: %d", current_buffer_pos);
            result_buffer[RESULT_BUFFER_SIZE - 1] = '\0'; // Ensure null termination at buffer end.
            current_buffer_pos = RESULT_BUFFER_SIZE - 1;
        } else {
            result_buffer[current_buffer_pos++] = ']'; // Close array
            result_buffer[current_buffer_pos++] = '}'; // Close main object
            result_buffer[current_buffer_pos] = '\0';  // Null terminate
        }
    } else if (result_buffer[current_buffer_pos-1] == ',') { // If there's a trailing comma from the last phrase object
        current_buffer_pos--; // Remove trailing comma before closing array.
        if (current_buffer_pos + required_len_finalize + 1 > RESULT_BUFFER_SIZE) {
            LOG_WARN("AVX Search: Result buffer too small to close JSON array after removing comma. Pos: %d", current_buffer_pos);
            result_buffer[RESULT_BUFFER_SIZE - 1] = '\0';
            current_buffer_pos = RESULT_BUFFER_SIZE - 1;
        } else {
            current_buffer_pos += snprintf(result_buffer + current_buffer_pos, RESULT_BUFFER_SIZE - current_buffer_pos, "]}");
        }
    } else if (result_buffer[current_buffer_pos-1] == '[') { // If array is open but empty (e.g. "{\"search_results\": [")
        // This case should be handled by is_empty_json, but as a fallback:
        result_buffer[current_buffer_pos++] = ']';
        result_buffer[current_buffer_pos++] = '}';
        result_buffer[current_buffer_pos] = '\0';
    }
    else { // Standard case: should be something like ...}]} (ending of a phrase object)
        // Add "]}" to close the search_results array and the main JSON object.
        if (current_buffer_pos + required_len_finalize + 1 > RESULT_BUFFER_SIZE) {
            LOG_WARN("AVX Search: Result buffer truncated or full while finalizing JSON. Pos: %d, Req: %d", current_buffer_pos, required_len_finalize);
            result_buffer[RESULT_BUFFER_SIZE - 1] = '\0';
            current_buffer_pos = RESULT_BUFFER_SIZE - 1;
        } else {
            result_buffer[current_buffer_pos++] = ']'; // Close array
            result_buffer[current_buffer_pos++] = '}'; // Close main object
            result_buffer[current_buffer_pos] = '\0';  // Null terminate
        }
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
