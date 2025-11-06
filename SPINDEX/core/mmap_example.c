/**
 * @file mmap_example.c
 * @brief Example usage of memory-mapped file reading
 *
 * Compile: gcc -o mmap_example mmap_example.c mmap_reader.c utils.c -I../include
 * Run: ./mmap_example /path/to/file
 */

#include "mmap_reader.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void demonstrate_mmap_reading(const char* filepath) {
    MmapFile file;

    printf("\n=== Memory-Mapped File Reading Demo ===\n");
    printf("File: %s\n\n", filepath);

    // Open and map the file
    if (!mmap_file_open(filepath, &file)) {
        fprintf(stderr, "Failed to open file with mmap\n");
        return;
    }

    printf("✓ File mapped successfully\n");
    printf("  Size: %zu bytes\n", mmap_file_size(&file));
    printf("  Mapped: %s\n", mmap_file_is_mapped(&file) ? "Yes" : "No");

    // Advise kernel about sequential access
    if (mmap_file_advise(&file, true)) {
        printf("✓ Advised kernel: sequential access\n");
    }

    // Access the data
    const char* data = (const char*)mmap_file_data(&file);
    size_t size = mmap_file_size(&file);

    if (data && size > 0) {
        printf("\n--- File Content Preview ---\n");

        // Show first 200 characters or entire file if smaller
        size_t preview_len = size < 200 ? size : 200;
        printf("%.*s\n", (int)preview_len, data);

        if (size > 200) {
            printf("\n... (%zu more bytes)\n", size - 200);
        }

        // Demonstrate direct memory access (very fast!)
        printf("\n--- Memory Access Performance ---\n");

        // Count newlines (example of processing)
        size_t newline_count = 0;
        for (size_t i = 0; i < size; i++) {
            if (data[i] == '\n') newline_count++;
        }
        printf("Lines in file: %zu\n", newline_count + 1);

        // Search for a pattern
        const char* pattern = "SPINDEX";
        size_t matches = 0;
        for (size_t i = 0; i <= size - strlen(pattern); i++) {
            if (memcmp(data + i, pattern, strlen(pattern)) == 0) {
                matches++;
            }
        }
        printf("Occurrences of '%s': %zu\n", pattern, matches);
    }

    // Clean up
    mmap_file_close(&file);
    printf("\n✓ File unmapped and closed\n");
}

void compare_performance(const char* filepath) {
    printf("\n=== Performance Comparison ===\n");
    printf("File: %s\n\n", filepath);

    // Method 1: Traditional read()
    printf("Method 1: Traditional read() syscalls\n");
    FILE* fp = fopen(filepath, "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char* buffer = malloc(size);
        if (buffer) {
            fread(buffer, 1, size, fp);
            printf("  Read %zu bytes using fread()\n", size);
            free(buffer);
        }
        fclose(fp);
    }

    // Method 2: Memory-mapped I/O
    printf("\nMethod 2: Memory-mapped I/O (mmap)\n");
    MmapFile file;
    if (mmap_file_open(filepath, &file)) {
        const char* data = mmap_file_data(&file);
        size_t size = mmap_file_size(&file);
        printf("  Mapped %zu bytes (zero-copy access!)\n", size);

        // Demonstrate that we can access data directly
        // No buffer allocation, no read() syscalls!
        if (data && size > 0) {
            printf("  First byte: 0x%02x\n", (unsigned char)data[0]);
        }

        mmap_file_close(&file);
    }

    printf("\n✓ mmap is faster because:\n");
    printf("  • No system call overhead per read\n");
    printf("  • No intermediate buffer copies\n");
    printf("  • Kernel handles page caching\n");
    printf("  • Perfect for large files\n");
}

#ifndef MMAP_EXAMPLE_NO_MAIN
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filepath>\n", argv[0]);
        fprintf(stderr, "\nExample: %s sample_data/readme.txt\n", argv[0]);
        return 1;
    }

    const char* filepath = argv[1];

    // Initialize logging
    spindex_log_init(NULL, LOG_LEVEL_INFO);

    // Demonstrate basic usage
    demonstrate_mmap_reading(filepath);

    // Show performance comparison
    compare_performance(filepath);

    spindex_log_shutdown();
    return 0;
}
#endif
