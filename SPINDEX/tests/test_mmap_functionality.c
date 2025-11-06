/**
 * @file test_mmap_functionality.c
 * @brief Test for memory-mapped I/O functionality
 *
 * This test verifies that the mmap_reader module works correctly.
 */

#include "mmap_reader.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_mmap_basic() {
    printf("Test 1: Basic mmap file opening\n");

    MmapFile file;
    const char* test_file = "sample_data/readme.txt";

    if (!mmap_file_open(test_file, &file)) {
        fprintf(stderr, "  ✗ Failed to open file: %s\n", test_file);
        return 0;
    }

    printf("  ✓ File opened successfully\n");

    // Check if mapped
    if (!mmap_file_is_mapped(&file)) {
        fprintf(stderr, "  ✗ File not mapped\n");
        mmap_file_close(&file);
        return 0;
    }

    printf("  ✓ File is mapped\n");

    // Check size
    size_t size = mmap_file_size(&file);
    if (size == 0) {
        fprintf(stderr, "  ✗ File size is 0\n");
        mmap_file_close(&file);
        return 0;
    }

    printf("  ✓ File size: %zu bytes\n", size);

    // Check data access
    const void* data = mmap_file_data(&file);
    if (data == NULL) {
        fprintf(stderr, "  ✗ Cannot access file data\n");
        mmap_file_close(&file);
        return 0;
    }

    printf("  ✓ Data accessible\n");

    mmap_file_close(&file);
    printf("  ✓ File closed successfully\n");

    return 1;
}

int test_mmap_advise() {
    printf("\nTest 2: madvise() for sequential access\n");

    MmapFile file;
    const char* test_file = "sample_data/readme.txt";

    if (!mmap_file_open(test_file, &file)) {
        fprintf(stderr, "  ✗ Failed to open file\n");
        return 0;
    }

    // Test sequential advice
    if (!mmap_file_advise(&file, true)) {
        fprintf(stderr, "  ✗ Sequential madvise() failed\n");
        mmap_file_close(&file);
        return 0;
    }

    printf("  ✓ Sequential access advice set\n");

    // Test random advice
    if (!mmap_file_advise(&file, false)) {
        fprintf(stderr, "  ✗ Random madvise() failed\n");
        mmap_file_close(&file);
        return 0;
    }

    printf("  ✓ Random access advice set\n");

    mmap_file_close(&file);
    return 1;
}

int test_mmap_content() {
    printf("\nTest 3: Content verification\n");

    MmapFile file;
    const char* test_file = "sample_data/readme.txt";

    if (!mmap_file_open(test_file, &file)) {
        fprintf(stderr, "  ✗ Failed to open file\n");
        return 0;
    }

    const char* data = (const char*)mmap_file_data(&file);
    size_t size = mmap_file_size(&file);

    // Check if content starts with expected text
    const char* expected_start = "SPINDEX";
    if (strncmp(data, expected_start, strlen(expected_start)) == 0) {
        printf("  ✓ Content starts with '%s'\n", expected_start);
    } else {
        fprintf(stderr, "  ✗ Content doesn't start with expected text\n");
        mmap_file_close(&file);
        return 0;
    }

    // Count newlines
    int newline_count = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') newline_count++;
    }

    printf("  ✓ Found %d lines in file\n", newline_count);

    mmap_file_close(&file);
    return 1;
}

int test_mmap_error_handling() {
    printf("\nTest 4: Error handling\n");

    MmapFile file;
    const char* nonexistent_file = "/tmp/nonexistent_file_12345.txt";

    if (mmap_file_open(nonexistent_file, &file)) {
        fprintf(stderr, "  ✗ Should have failed to open nonexistent file\n");
        mmap_file_close(&file);
        return 0;
    }

    printf("  ✓ Correctly failed to open nonexistent file\n");

    return 1;
}

int main() {
    printf("=== Memory-Mapped I/O Functionality Tests ===\n\n");

    int tests_passed = 0;
    int tests_total = 4;

    if (test_mmap_basic()) tests_passed++;
    if (test_mmap_advise()) tests_passed++;
    if (test_mmap_content()) tests_passed++;
    if (test_mmap_error_handling()) tests_passed++;

    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_total);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_total - tests_passed);

    if (tests_passed == tests_total) {
        printf("\n✅ All mmap tests PASSED\n");
        return 0;
    } else {
        printf("\n❌ Some mmap tests FAILED\n");
        return 1;
    }
}
