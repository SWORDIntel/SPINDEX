# Test Plan: [AVX-001] AVX-512 Phrase Search Algorithm

## 1. Overview

This document outlines the test plan for the AVX-512 Phrase Search plugin (`avx512_phrase_search.c`). The goal is to ensure the plugin's correctness, robustness, and adherence to its design specifications.

## 2. Test Environment

*   **CPU:** Requires a CPU supporting AVX-512F and AVX-512BW instructions. Tests should ideally run on such hardware.
*   **OS:** Linux (development/testing environment).
*   **Compiler:** GCC (as per project Makefile).
*   **Build System:** Project Makefile.
*   **Testing Framework:** Custom C test functions (e.g., using `assert.h`) or a lightweight C unit testing framework. The test runner will be a dedicated C executable.

## 3. Test Harness

A C program (`test_avx512_phrase_search.c`) will be created to:
*   Load the `avx512_phrase_search.so` plugin dynamically (if testing the .so) OR link directly against `avx512_phrase_search.o` and its dependencies for unit testing. The latter is preferred for fine-grained unit tests.
*   Prepare `AvxSearchParams` structures with various `ProcessedDataChunk` inputs for text and phrases.
*   Call the plugin's functions: `avx512_init()`, `avx512_process_data()`, and `avx512_shutdown()`.
*   Verify the output `ProcessedDataChunk` (JSON string) against expected results.
*   Check for memory leaks or corruption (e.g., using Valgrind).

## 4. Test Cases

### 4.1. Initialization and Shutdown

*   **TC-INIT-001:** `avx512_init()` succeeds on a CPU with AVX-512F and AVX-512BW support.
    *   Expected: Returns 0.
*   **TC-INIT-002:** `avx512_init()` fails on a CPU without AVX-512F or AVX-512BW support.
    *   Expected: Returns -1. (Requires mocking or hardware without support).
*   **TC-INIT-003:** `avx512_shutdown()` runs without errors after successful initialization.
    *   Expected: No crashes or memory issues.

### 4.2. Basic Functionality

*   **TC-FUNC-001: Phrase Not Found**
    *   Text: "This is a sample text."
    *   Phrases: "xyz\\nsearch" (newline separated)
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "xyz", "offsets": []}, {"phrase": "search", "offsets": []}]}`
*   **TC-FUNC-002: Phrase Found (Single Occurrence)**
    *   Text: "This is a sample text for testing."
    *   Phrases: "sample text"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "sample text", "offsets": [10]}]}`
*   **TC-FUNC-003: Phrase Found (Multiple Occurrences)**
    *   Text: "test phrase, another test phrase."
    *   Phrases: "test phrase"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "test phrase", "offsets": [0, 20]}]}`
*   **TC-FUNC-004: Multiple Phrases (Found and Not Found)**
    *   Text: "The quick brown fox jumps over the lazy dog."
    *   Phrases: "quick brown\\nlazy dog\\nmissing phrase"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "quick brown", "offsets": [4]}, {"phrase": "lazy dog", "offsets": [35]}, {"phrase": "missing phrase", "offsets": []}]}`

### 4.3. Case Sensitivity

*   **TC-CASE-001: Case Insensitive Match**
    *   Text: "Sample TeXT"
    *   Phrases: "sample text"
    *   `case_sensitive`: false
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "sample text", "offsets": [0]}]}`
*   **TC-CASE-002: Case Sensitive No Match (due to case)**
    *   Text: "Sample TeXT"
    *   Phrases: "sample text"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "sample text", "offsets": []}]}`
*   **TC-CASE-003: Case Sensitive Match**
    *   Text: "sample text"
    *   Phrases: "sample text"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "sample text", "offsets": [0]}]}`

### 4.4. `find_all_occurrences` Parameter

*   **TC-FINDALL-001: `find_all_occurrences` is false**
    *   Text: "test phrase, another test phrase."
    *   Phrases: "test phrase"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: false
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "test phrase", "offsets": [0]}]}` (Only first occurrence)
*   **TC-FINDALL-002: `find_all_occurrences` is true (covered by TC-FUNC-003)**

### 4.5. `max_hits_per_doc` Parameter

*   **TC-MAXHITS-001: `max_hits_per_doc` limits results**
    *   Text: "hit hit hit hit hit"
    *   Phrases: "hit"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 3
    *   Expected JSON: `{"search_results": [{"phrase": "hit", "offsets": [0, 4, 8]}]}`
*   **TC-MAXHITS-002: `max_hits_per_doc` is 0 (no limit)**
    *   Text: "hit hit hit"
    *   Phrases: "hit"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Expected JSON: `{"search_results": [{"phrase": "hit", "offsets": [0, 4, 8]}]}`
*   **TC-MAXHITS-003: `max_hits_per_doc` is negative (no limit)**
    *   Text: "hit hit hit"
    *   Phrases: "hit"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: -1
    *   Expected JSON: `{"search_results": [{"phrase": "hit", "offsets": [0, 4, 8]}]}`
*   **TC-MAXHITS-004: `max_hits_per_doc` with `find_all_occurrences` false**
    *   Text: "hit hit hit hit hit"
    *   Phrases: "hit"
    *   `case_sensitive`: true
    *   `find_all_occurrences`: false
    *   `max_hits_per_doc`: 3
    *   Expected JSON: `{"search_results": [{"phrase": "hit", "offsets": [0]}]}` (Still only one due to `find_all_occurrences` being false)

### 4.6. Edge Cases: Input Data

*   **TC-EDGE-001: Empty Text**
    *   Text: ""
    *   Phrases: "test"
    *   Expected JSON: `{"search_results": [{"phrase": "test", "offsets": []}]}`
*   **TC-EDGE-002: Empty Phrases String**
    *   Text: "some text"
    *   Phrases: "" (empty string)
    *   Expected JSON: `{"search_results": []}` (No phrases to process)
*   **TC-EDGE-003: Phrases String with only Newlines**
    *   Text: "some text"
    *   Phrases: "\\n\\n"
    *   Expected JSON: `{"search_results": []}` as empty phrases are skipped by `strtok_r` and `phrase_len == 0` check.
*   **TC-EDGE-004: Phrase Length Equal to Text Length (Match)**
    *   Text: "exactmatch"
    *   Phrases: "exactmatch"
    *   Expected JSON: `{"search_results": [{"phrase": "exactmatch", "offsets": [0]}]}`
*   **TC-EDGE-005: Phrase Length Equal to Text Length (No Match)**
    *   Text: "exactmatch"
    *   Phrases: "nomatch..." (same length)
    *   Expected JSON: `{"search_results": [{"phrase": "nomatch...", "offsets": []}]}`
*   **TC-EDGE-006: Phrase Length Greater Than Text Length**
    *   Text: "short"
    *   Phrases: "longerphrase"
    *   Expected JSON: `{"search_results": [{"phrase": "longerphrase", "offsets": []}]}`
*   **TC-EDGE-007: Text/Phrase Lengths around AVX_BLOCK_SIZE (64 bytes)**
    *   Test with text/phrases of lengths: 63, 64, 65, 127, 128, 129.
    *   Vary `case_sensitive` and content to ensure no boundary condition errors.
    *   E.g., Phrase: "A...A" (63 As), Text: "B...BA...AB...B" (Bs then 63 As then Bs) -> Expected: one match
    *   E.g., Phrase: "A...A" (64 As), Text: "B...BA...AB...B" (Bs then 64 As then Bs) -> Expected: one match
*   **TC-EDGE-008: Phrases at Start/End of Text**
    *   Text: "start middle end"
    *   Phrases: "start\\nmiddle\\nend"
    *   Expected JSON: `{"search_results": [{"phrase": "start", "offsets": [0]}, {"phrase": "middle", "offsets": [6]}, {"phrase": "end", "offsets": [13]}]}`
*   **TC-EDGE-009: Overlapping Potential Matches (Non-overlapping actual phrase occurrences)**
    *   Text: "abababa"
    *   Phrases: "ababa"
    *   `find_all_occurrences`: true
    *   Expected JSON: `{"search_results": [{"phrase": "ababa", "offsets": [0, 2]}]}`

### 4.7. Special Characters and JSON Escaping

*   **TC-JSON-001: Text/Phrases with Quotes and Backslashes**
    *   Text: "Text with \\"quotes\\" and backslash \\\\."
    *   Phrases: "\\"quotes\\"\\nbackslash \\\\"
    *   Expected JSON: `{"search_results": [{"phrase": "\\\"quotes\\\"", "offsets": [11]}, {"phrase": "backslash \\\\", "offsets": [27]}]}` (Verify JSON validity and correct escaping)
*   **TC-JSON-002: Text/Phrases with Control Characters**
    *   Text: "Text with \t tab and \n newline and \x01 SOH." (SOH char as actual byte \x01 in C test string)
    *   Phrases: "\t tab\\n\\n newline\\n\x01 SOH" (SOH char as actual byte \x01 in C test string)
    *   Expected JSON: `{"search_results": [{"phrase": "\\t tab", "offsets": [10]}, {"phrase": "\\n newline", "offsets": [20]}, {"phrase": "\\u0001 SOH", "offsets": [34]}]}` (Verify JSON validity and correct `\uXXXX` escaping for SOH)
*   **TC-JSON-003: Text/Phrases with all JSON special chars**
    *   Text: "Test: \" \\\\ / \b \f \n \r \t end"
    *   Phrases: "\" \\\\ / \b \f \n \r \t"
    *   Expected JSON: `{"search_results": [{"phrase": "\\\" \\\\ / \\b \\f \\n \\r \\t", "offsets": [6]}]}` (Note: `/` doesn't need escaping by default in JSON strings. Our `escape_json_string` does not escape `/`.)

### 4.8. Buffer Overflow Handling (JSON Result)

*   **TC-BUFF-001: Many Small Matches Exceeding Buffer**
    *   Text: A very long string composed of "a a a a a ..."
    *   Phrases: "a"
    *   `find_all_occurrences`: true
    *   `max_hits_per_doc`: 0
    *   Set `RESULT_BUFFER_SIZE` (compile-time constant for the plugin, or a runtime parameter if plugin is adapted) to a small value for testing this.
    *   Expected: Plugin should not crash. Output JSON should be truncated safely (e.g., valid JSON up to the point of truncation, possibly ending with `...` or a specific error message in logs). Check logs for warnings about truncation.
*   **TC-BUFF-002: Long Phrase Exceeding Buffer for Header**
    *   Text: "some text"
    *   Phrases: A very long phrase string that itself, when escaped, would overflow the buffer when constructing `{"phrase": "...", "offsets": [`.
    *   Expected: Plugin should not crash. Logs should indicate truncation. JSON output might be minimal like `{"search_results": []}` or `{"search_results": [{/*truncated entry*/}]}` if it can manage.

### 4.9. Null/Invalid Inputs (Defensive Checks)

*   **TC-NULL-001: NULL `input_chunk`**
    *   Call `avx512_process_data(NULL)`.
    *   Expected: Returns `NULL`. Logs error. No crash.
*   **TC-NULL-002: `input_chunk->data` is NULL**
    *   `ProcessedDataChunk wrapper_chunk = {.data = NULL, .length = 0};`
    *   Call `avx512_process_data(&wrapper_chunk)`.
    *   Expected: Returns `NULL`. Logs error. No crash.
*   **TC-NULL-003: `params->text_to_search` is NULL**
    *   `ProcessedDataChunk phrases_chunk = {.data = (void*)"phrase", .length = strlen("phrase"), .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "test_phrase_chunk"};`
    *   `AvxSearchParams params_instance = {.text_to_search = NULL, .phrases_to_find = &phrases_chunk, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0};`
    *   `ProcessedDataChunk wrapper_chunk = {.data = &params_instance, .length = sizeof(AvxSearchParams), .data_type = CHUNK_TYPE_OPAQUE_PARAMS, .source_id = "wrapper"};`
    *   Call `avx512_process_data(&wrapper_chunk)`.
    *   Expected: Returns `NULL`. Logs error. No crash.
*   **TC-NULL-004: `params->phrases_to_find` is NULL**
    *   `ProcessedDataChunk text_chunk = {.data = (void*)"text", .length = strlen("text"), .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "test_text_chunk"};`
    *   `AvxSearchParams params_instance = {.text_to_search = &text_chunk, .phrases_to_find = NULL, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0};`
    *   `ProcessedDataChunk wrapper_chunk = {.data = &params_instance, .length = sizeof(AvxSearchParams), .data_type = CHUNK_TYPE_OPAQUE_PARAMS, .source_id = "wrapper"};`
    *   Call `avx512_process_data(&wrapper_chunk)`.
    *   Expected: Returns `NULL`. Logs error. No crash.
*   **TC-NULL-005: `params->text_to_search->data` is NULL**
    *   `ProcessedDataChunk text_chunk_with_null_data = {.data = NULL, .length = 0, .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "text_null_data"};`
    *   `ProcessedDataChunk phrases_chunk = {.data = (void*)"phrase", .length = strlen("phrase"), .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "test_phrase_chunk"};`
    *   `AvxSearchParams params_instance = {.text_to_search = &text_chunk_with_null_data, .phrases_to_find = &phrases_chunk, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0};`
    *   `ProcessedDataChunk wrapper_chunk = {.data = &params_instance, .length = sizeof(AvxSearchParams), .data_type = CHUNK_TYPE_OPAQUE_PARAMS, .source_id = "wrapper"};`
    *   Call `avx512_process_data(&wrapper_chunk)`.
    *   Expected: Returns `NULL`. Logs error. No crash.
*   **TC-NULL-006: `params->phrases_to_find->data` is NULL**
    *   `ProcessedDataChunk text_chunk = {.data = (void*)"text", .length = strlen("text"), .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "test_text_chunk"};`
    *   `ProcessedDataChunk phrases_chunk_with_null_data = {.data = NULL, .length = 0, .data_type = CHUNK_TYPE_UTF8_TEXT, .source_id = "phrase_null_data"};`
    *   `AvxSearchParams params_instance = {.text_to_search = &text_chunk, .phrases_to_find = &phrases_chunk_with_null_data, .case_sensitive = true, .find_all_occurrences = true, .max_hits_per_doc = 0};`
    *   `ProcessedDataChunk wrapper_chunk = {.data = &params_instance, .length = sizeof(AvxSearchParams), .data_type = CHUNK_TYPE_OPAQUE_PARAMS, .source_id = "wrapper"};`
    *   Call `avx512_process_data(&wrapper_chunk)`.
    *   Expected: Returns `NULL`. Logs error. No crash.

## 5. Test Execution and Reporting

*   Tests will be executed via `make test_avx512_phrase_search` (or similar target).
*   Results (pass/fail for each test case) will be printed to standard output.
*   Valgrind will be used periodically to check for memory issues.
*   A summary of test results will be included in commit messages or PRs.

## 6. Dependencies

*   `hardware_detection.c` (for CPU feature check)
*   `utils.c` (for logging)
*   `data_types.h`, `plugin_interface.h`, `avx_search_params.h`
