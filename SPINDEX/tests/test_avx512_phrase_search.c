#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

#include "../../include/plugin_interface.h"
#include "../../include/data_types.h"
#include "../../include/avx_search_params.h"
#include "../../include/utils.h"

// Forward declaration for the function we are testing
ProcessedDataChunk* avx512_process_data(const ProcessedDataChunk *input_chunk);

// --- Test Utilities ---
static int test_count = 0;
static int tests_passed = 0;

#define ASSERT_NOT_NULL(ptr) \
    do { \
        test_count++; \
        if ((ptr) != NULL) { tests_passed++; } \
        else { printf("TEST FAILED: %s (%s:%d) - pointer is NULL\n", __func__, __FILE__, __LINE__); } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        test_count++; \
        if ((ptr) == NULL) { tests_passed++; } \
        else { printf("TEST FAILED: %s (%s:%d) - pointer is not NULL\n", __func__, __FILE__, __LINE__); } \
    } while (0)

#define ASSERT_STR_EQ(str1, str2) \
    do { \
        test_count++; \
        if (strcmp((str1), (str2)) == 0) { tests_passed++; } \
        else { printf("TEST FAILED: %s (%s:%d) -\n  Expected: \"%s\"\n  Got:      \"%s\"\n", __func__, __FILE__, __LINE__, (str2), (str1)); } \
    } while (0)

// --- Mock implementations for dependencies ---
void sp_log(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

char* sp_strdup(const char* s) {
    if (!s) return NULL;
    char* new_s = malloc(strlen(s) + 1);
    if (new_s) strcpy(new_s, s);
    return new_s;
}

// --- Test Cases ---

void test_phrase_not_found() {
    const char* text = "This is a sample text.";
    const char* phrases = "xyz\nsearch";
    ProcessedDataChunk text_chunk = { .data = (unsigned char*)text, .length = strlen(text), .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "test1" };
    ProcessedDataChunk phrases_chunk = { .data = (unsigned char*)phrases, .length = strlen(phrases), .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "test1_phrases" };
    AvxSearchParams params = { .text_to_search = &text_chunk, .phrases_to_find = &phrases_chunk, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0 };
    ProcessedDataChunk input_chunk = { .data = (unsigned char*)&params, .length = sizeof(params) };

    ProcessedDataChunk* result = avx512_process_data(&input_chunk);
    ASSERT_NOT_NULL(result);
    if (result) {
        ASSERT_NOT_NULL(result->data);
        if (result->data) {
            const char* expected_json = "{\"search_results\": [{\"phrase\": \"xyz\", \"offsets\": []},{\"phrase\": \"search\", \"offsets\": []}]}";
            ASSERT_STR_EQ((char*)result->data, expected_json);
        }
        free(result->data);
        free(result);
    }
}

void test_single_occurrence() {
    const char* text = "This is a sample text for testing.";
    const char* phrases = "sample text";
    ProcessedDataChunk text_chunk = { .data = (unsigned char*)text, .length = strlen(text)};
    ProcessedDataChunk phrases_chunk = { .data = (unsigned char*)phrases, .length = strlen(phrases)};
    AvxSearchParams params = { .text_to_search = &text_chunk, .phrases_to_find = &phrases_chunk, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0 };
    ProcessedDataChunk input_chunk = { .data = (unsigned char*)&params, .length = sizeof(params) };

    ProcessedDataChunk* result = avx512_process_data(&input_chunk);
    ASSERT_NOT_NULL(result);
    if (result) {
        const char* expected_json = "{\"search_results\": [{\"phrase\": \"sample text\", \"offsets\": [10]}]}";
        ASSERT_STR_EQ((char*)result->data, expected_json);
        free(result->data);
        free(result);
    }
}

void test_multiple_occurrences() {
    const char* text = "test phrase, another test phrase.";
    const char* phrases = "test phrase";
    ProcessedDataChunk text_chunk = { .data = (unsigned char*)text, .length = strlen(text)};
    ProcessedDataChunk phrases_chunk = { .data = (unsigned char*)phrases, .length = strlen(phrases)};
    AvxSearchParams params = { .text_to_search = &text_chunk, .phrases_to_find = &phrases_chunk, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0 };
    ProcessedDataChunk input_chunk = { .data = (unsigned char*)&params, .length = sizeof(params) };

    ProcessedDataChunk* result = avx512_process_data(&input_chunk);
    ASSERT_NOT_NULL(result);
    if (result) {
        const char* expected_json = "{\"search_results\": [{\"phrase\": \"test phrase\", \"offsets\": [0,21]}]}";
        ASSERT_STR_EQ((char*)result->data, expected_json);
        free(result->data);
        free(result);
    }
}

void test_case_insensitive() {
    const char* text = "Sample TeXT";
    const char* phrases = "sample text";
    ProcessedDataChunk text_chunk = { .data = (unsigned char*)text, .length = strlen(text)};
    ProcessedDataChunk phrases_chunk = { .data = (unsigned char*)phrases, .length = strlen(phrases)};
    AvxSearchParams params = { .text_to_search = &text_chunk, .phrases_to_find = &phrases_chunk, .case_sensitive = false, .find_all_occurrences = true, .max_hits_per_doc = 0 };
    ProcessedDataChunk input_chunk = { .data = (unsigned char*)&params, .length = sizeof(params) };

    ProcessedDataChunk* result = avx512_process_data(&input_chunk);
    ASSERT_NOT_NULL(result);
    if (result) {
        const char* expected_json = "{\"search_results\": [{\"phrase\": \"sample text\", \"offsets\": [0]}]}";
        ASSERT_STR_EQ((char*)result->data, expected_json);
        free(result->data);
        free(result);
    }
}


void test_max_hits() {
    const char* text = "hit hit hit hit hit";
    const char* phrases = "hit";
    ProcessedDataChunk text_chunk = { .data = (unsigned char*)text, .length = strlen(text)};
    ProcessedDataChunk phrases_chunk = { .data = (unsigned char*)phrases, .length = strlen(phrases)};
    AvxSearchParams params = { .text_to_search = &text_chunk, .phrases_to_find = &phrases_chunk, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 3 };
    ProcessedDataChunk input_chunk = { .data = (unsigned char*)&params, .length = sizeof(params) };

    ProcessedDataChunk* result = avx512_process_data(&input_chunk);
    ASSERT_NOT_NULL(result);
    if (result) {
        const char* expected_json = "{\"search_results\": [{\"phrase\": \"hit\", \"offsets\": [0,4,8]}]}";
        ASSERT_STR_EQ((char*)result->data, expected_json);
        free(result->data);
        free(result);
    }
}


// --- Main Test Runner ---
int main() {
    printf("--- Running AVX-512 Phrase Search Plugin Tests ---\n");

    test_phrase_not_found();
    test_single_occurrence();
    test_multiple_occurrences();
    test_case_insensitive();
    test_max_hits();

    printf("--- Test Summary ---\n");
    printf("%d/%d tests passed.\n", tests_passed, test_count);
    return (tests_passed == test_count) ? 0 : 1;
}
