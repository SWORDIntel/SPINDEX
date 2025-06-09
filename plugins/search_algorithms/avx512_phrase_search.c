#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <immintrin.h> // For AVX-512 intrinsics

#include "plugin_interface.h"
#include "avx_search_params.h" // Assumed to be in include path by Makefile
#include "utils.h"             // For sp_strdup, sp_log, etc.
#include "hardware_detection.h" // For is_avx512_supported

#define MAX_PHRASE_LEN 256
#define MAX_TOKEN_LEN 64
#define MAX_TOKENS_PER_PHRASE 10

// Forward declaration if not in a shared header accessible by utils.c
// For simplicity, assume sp_log is available via utils.h which is included.

// Plugin metadata
PluginInfo plugin_info = {
    "AVX-512 Phrase Search",
    "0.2",
    "Performs multi-token phrase search using AVX-512 optimizations where applicable.",
    PLUGIN_TYPE_SEARCH_ALGORITHM
};

// Helper to tokenize a single phrase
// Modifies the input string by replacing delimiters with null terminators
static int tokenize_phrase(char* phrase_str, char tokens[][MAX_TOKEN_LEN], int max_tokens) {
    int count = 0;
    char* token = strtok(phrase_str, " "); // Assuming space as delimiter
    while (token != NULL && count < max_tokens) {
        strncpy(tokens[count], token, MAX_TOKEN_LEN - 1);
        tokens[count][MAX_TOKEN_LEN - 1] = '\0'; // Ensure null termination
        count++;
        token = strtok(NULL, " ");
    }
    return count;
}

// Basic AVX-512 accelerated search for a single token (substring)
// Returns a pointer to the first occurrence, or NULL if not found
static const char* find_substring_avx512(const char* text, size_t text_len, const char* token, size_t token_len) {
    if (!text || !token || token_len == 0 || text_len < token_len) {
        return NULL;
    }

    // Simplified AVX-512 search:
    // This example primarily accelerates finding the first character of the token.
    // A more advanced implementation would try to match larger parts of the token.

    char first_char = token[0];
    __m512i first_char_vec = _mm512_set1_epi8(first_char);

    for (size_t i = 0; i <= text_len - token_len; i += 64) {
        size_t current_chunk_len = (text_len - i < 64) ? (text_len - i) : 64;
        if (current_chunk_len < token_len) break; // Not enough space left for the token

        __mmask64 cmp_mask = 0;
        // Only load and compare up to current_chunk_len to avoid reading past text_len
        if (current_chunk_len == 64) {
            __m512i text_vec = _mm512_loadu_si512((const __m512i*)(text + i));
            cmp_mask = _mm512_cmpeq_epi8_mask(text_vec, first_char_vec);
        } else {
            // Handle partial last chunk - create a masked load or compare character by character
            // For simplicity in this example, we'll rely on the outer loop's bounds
            // and the strncmp for the final check for partial chunks.
            // A production version would use masked loads/compares or pad carefully.
             __m512i text_vec = _mm512_maskz_loadu_epi8((1ULL << current_chunk_len) - 1, (const __m512i*)(text + i));
             cmp_mask = _mm512_cmpeq_epi8_mask(text_vec, first_char_vec);
        }


        unsigned long match_indices = cmp_mask;
        while (match_indices > 0) {
            unsigned long bit_pos = _tzcnt_u64(match_indices); // Index of the LSB set
            if (i + bit_pos <= text_len - token_len) {
                if (strncmp(text + i + bit_pos, token, token_len) == 0) {
                    return text + i + bit_pos;
                }
            }
            match_indices &= (match_indices - 1); // Clear LSB
        }
    }
    // Fallback for any remaining part or if no AVX match found quickly
    // This part can also be hit if text_len is small.
    // A simple strstr can be a fallback or used for non-AVX paths.
    // For this example, the loop above should cover it, but a robust solution might need more.
    // For remaining part less than 64 bytes not fully covered by AVX loop:
    size_t avx_covered_len = (text_len / 64) * 64;
    if (avx_covered_len < text_len) {
        const char* p = strstr(text + avx_covered_len, token);
        if (p && (p + token_len <= text + text_len) ) return p;
    }

    return NULL;
}


ProcessedDataChunk* process_data(const ProcessedDataChunk *input_chunk) {
    if (!is_avx512_supported()) {
        sp_log(LOG_LEVEL_ERROR, "AVX-512 not supported on this hardware. Plugin cannot operate.");
        // Return an error chunk or NULL. Let's return an error message.
        ProcessedDataChunk* error_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
        if (!error_chunk) return NULL;
        error_chunk->data_type = CHUNK_TYPE_TEXT; // Or a specific error type
        char* err_msg = sp_strdup("AVX-512 not supported.");
        if (!err_msg) { free(error_chunk); return NULL; }
        error_chunk->data = (unsigned char*)err_msg;
        error_chunk->length = strlen(err_msg);
        error_chunk->metadata = NULL;
        return error_chunk;
    }

    if (!input_chunk || !input_chunk->data) {
        sp_log(LOG_LEVEL_ERROR, "AVX512_Search: Input chunk or data is NULL.");
        return NULL;
    }

    const AvxSearchParams* params = (const AvxSearchParams*)input_chunk->data;
    if (!params || !params->text_to_search || !params->phrases_to_find ||
        !params->text_to_search->data || !params->phrases_to_find->data) {
        sp_log(LOG_LEVEL_ERROR, "AVX512_Search: Invalid search parameters or data within parameters is NULL.");
        return NULL;
    }

    const char* text_to_search = (const char*)params->text_to_search->data;
    size_t text_len = params->text_to_search->length;

    // Copy phrases_to_find data as it will be modified by tokenize_phrase (strtok)
    char* phrases_buffer = sp_strdup((const char*)params->phrases_to_find->data);
    if (!phrases_buffer) {
        sp_log(LOG_LEVEL_ERROR, "AVX512_Search: Failed to duplicate phrases buffer.");
        return NULL;
    }

    int phrases_found_count = 0;
    char* current_phrase_str = phrases_buffer;
    char* next_phrase_str = NULL;

    // Iterate through newline-separated phrases
    // strtok_r is safer for reentrancy if needed, but for simple parsing here,
    // we are processing one phrase line at a time from the duplicated buffer.
    char *phrase_line = strtok_r(phrases_buffer, "\n", &next_phrase_str);

    while(phrase_line != NULL) {
        if (strlen(phrase_line) == 0) { // Skip empty lines
            phrase_line = strtok_r(NULL, "\n", &next_phrase_str);
            continue;
        }
        sp_log(LOG_LEVEL_DEBUG, "AVX512_Search: Processing phrase: [%s]", phrase_line);

        char tokens[MAX_TOKENS_PER_PHRASE][MAX_TOKEN_LEN];
        // Duplicate phrase_line for strtok since tokenize_phrase modifies its input
        char* phrase_line_copy = sp_strdup(phrase_line);
        if (!phrase_line_copy) {
             sp_log(LOG_LEVEL_WARN, "AVX512_Search: Could not duplicate phrase line for tokenization.");
             phrase_line = strtok_r(NULL, "\n", &next_phrase_str);
             continue;
        }

        int num_tokens = tokenize_phrase(phrase_line_copy, tokens, MAX_TOKENS_PER_PHRASE);
        free(phrase_line_copy); // Free the copy

        if (num_tokens == 0) {
            sp_log(LOG_LEVEL_WARN, "AVX512_Search: Phrase produced no tokens: [%s]", phrase_line);
            phrase_line = strtok_r(NULL, "\n", &next_phrase_str);
            continue;
        }

        const char* current_search_pos = text_to_search;
        size_t remaining_text_len = text_len;
        bool current_phrase_found_once = false;

        while(true) { // Loop to find all occurrences of the first token
            const char* first_token_match = find_substring_avx512(current_search_pos, remaining_text_len, tokens[0], strlen(tokens[0]));

            if (!first_token_match) {
                break; // First token not found (anymore) in the remaining text
            }

            // First token found, now check subsequent tokens
            bool sequence_match = true;
            if (num_tokens > 1) {
                const char* next_expected_pos = first_token_match + strlen(tokens[0]);
                // Expect a space or end of text after the token
                if (*next_expected_pos != ' ' && *next_expected_pos != '\0' && *next_expected_pos != '\n' && *next_expected_pos != '\r') {
                    sequence_match = false; // Not a whole word match for the first token
                } else {
                    if (*next_expected_pos == ' ') next_expected_pos++; // Skip the space

                    for (int k = 1; k < num_tokens; ++k) {
                        size_t current_token_len = strlen(tokens[k]);
                        if ((size_t)(text_to_search + text_len - next_expected_pos) < current_token_len ||
                            strncmp(next_expected_pos, tokens[k], current_token_len) != 0) {
                            sequence_match = false;
                            break;
                        }
                        next_expected_pos += current_token_len;
                        // Expect a space or end of text/line after intermediate tokens too
                        if (k < num_tokens - 1) { // If not the last token in phrase
                           if (*next_expected_pos != ' ' && *next_expected_pos != '\0' && *next_expected_pos != '\n' && *next_expected_pos != '\r'){
                                sequence_match = false; break;
                           }
                           if (*next_expected_pos == ' ') next_expected_pos++;
                        }
                    }
                }
            }

            if (sequence_match) {
                current_phrase_found_once = true;
                // For this subtask, we only count if the phrase is found at least once.
                // To find all occurrences of the phrase, we would continue searching for the first token.
                // For now, if one phrase is found, we break and count it.
                break;
            }

            // If not a sequence match, or if we wanted all occurrences, advance search for the first token
            size_t advance_by = (first_token_match - current_search_pos) + 1; // Advance by at least 1 char
            remaining_text_len -= advance_by;
            current_search_pos = first_token_match + 1;
            if (remaining_text_len < strlen(tokens[0])) break; // Not enough text left for the first token
        }

        if (current_phrase_found_once) {
            phrases_found_count++;
            sp_log(LOG_LEVEL_INFO, "AVX512_Search: Found phrase: [%s]", phrase_line);
        }

        phrase_line = strtok_r(NULL, "\n", &next_phrase_str);
    }

    free(phrases_buffer);

    // Prepare output chunk
    ProcessedDataChunk* output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        sp_log(LOG_LEVEL_FATAL, "AVX512_Search: Failed to allocate output chunk.");
        return NULL;
    }
    output_chunk->data_type = CHUNK_TYPE_TEXT; // Or a custom result type

    char result_buffer[128];
    snprintf(result_buffer, sizeof(result_buffer), "{\"phrases_found_count\": %d}", phrases_found_count);

    output_chunk->data = (unsigned char*)sp_strdup(result_buffer);
    if (!output_chunk->data) {
        sp_log(LOG_LEVEL_FATAL, "AVX512_Search: Failed to allocate data for output chunk.");
        free(output_chunk);
        return NULL;
    }
    output_chunk->length = strlen(result_buffer);
    output_chunk->metadata = NULL; // No specific metadata for this simple result

    sp_log(LOG_LEVEL_INFO, "AVX512_Search: Processing complete. Found %d phrases.", phrases_found_count);
    return output_chunk;
}

// Plugin registration (not strictly part of this interface, but for completeness)
int spindex_plugin_init() {
    sp_log(LOG_LEVEL_INFO, "AVX-512 Phrase Search Plugin Initialized.");
    if (!is_avx512_supported()) {
         sp_log(LOG_LEVEL_WARN, "AVX-512 is NOT supported on this CPU. Plugin will not perform optimally or may error.");
    } else {
         sp_log(LOG_LEVEL_INFO, "AVX-512 is supported on this CPU.");
    }
    return 0; // Success
}

void spindex_plugin_shutdown() {
    sp_log(LOG_LEVEL_INFO, "AVX-512 Phrase Search Plugin Shutdown.");
}

void spindex_plugin_get_info(PluginInfo *info) {
    if (info) {
        *info = plugin_info;
    }
}
