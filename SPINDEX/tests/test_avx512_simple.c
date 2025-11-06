/**
 * @file test_avx512_simple.c
 * @brief Simple test for AVX512 plugin symbol verification
 *
 * This test verifies that the AVX512 plugin is built correctly
 * and has all required symbols.
 */

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

int main() {
    printf("=== AVX512 Plugin Symbol Test ===\n\n");

    const char* plugin_path = "build_cmake/lib/spindex/plugins/avx512_phrase_search.so";

    // Try to load the plugin
    void* handle = dlopen(plugin_path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "FAIL: Could not load AVX512 plugin: %s\n", dlerror());
        return 1;
    }

    printf("✓ Plugin loaded: %s\n\n", plugin_path);

    // Check for required symbols
    const char* required_symbols[] = {
        "spindex_plugin_get_info",
        "spindex_plugin_get_dependencies",
        "spindex_plugin_free_dependencies_list",
        "avx512_init",
        "avx512_process_data",
        "avx512_shutdown",
        "avx512_find_substring",
        NULL
    };

    int passed = 0;
    int failed = 0;

    for (int i = 0; required_symbols[i] != NULL; i++) {
        void* symbol = dlsym(handle, required_symbols[i]);
        if (symbol) {
            printf("✓ Symbol found: %s\n", required_symbols[i]);
            passed++;
        } else {
            printf("✗ Symbol missing: %s\n", required_symbols[i]);
            failed++;
        }
    }

    dlclose(handle);

    printf("\n=== Test Summary ===\n");
    printf("Symbols checked: %d\n", passed + failed);
    printf("Found: %d\n", passed);
    printf("Missing: %d\n", failed);

    if (failed == 0) {
        printf("\n✅ AVX512 plugin test PASSED\n");
        return 0;
    } else {
        printf("\n❌ AVX512 plugin test FAILED\n");
        return 1;
    }
}
