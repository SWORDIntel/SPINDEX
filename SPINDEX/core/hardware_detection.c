#include "hardware_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Helper function to check for a substring (feature flag) in the flags line
// Case-sensitive, /proc/cpuinfo flags are usually lowercase.
static bool check_flag(const char *flags_line, const char *flag) {
    if (!flags_line || !flag) {
        return false;
    }
    // strstr is sufficient if flags are space-separated and we check for " flag " or " flag\n"
    // A more robust way might involve tokenizing, but this is simpler for now.
    // We look for " flag" to avoid matching substrings (e.g., "ssse3" if we only search "sse")
    // and ensure it's a whole word. Add space at the beginning of flag.
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), " %s", flag); // e.g., " sse"

    const char *found = strstr(flags_line, search_pattern);
    return found != NULL;
}


void init_cpu_features(CpuFeatures *features) {
    if (!features) return;
    memset(features, 0, sizeof(CpuFeatures)); // Zero out the structure
    features->num_cores = 0; // Explicitly set to 0, though memset does this.
    features->has_sse = false;
    features->has_sse2 = false;
    features->has_avx = false;
    features->has_avx2 = false;
    features->flags_line[0] = '\0';
}

int detect_cpu_features(CpuFeatures *features) {
    if (!features) {
        return -1;
    }
    init_cpu_features(features); // Initialize to defaults

    FILE *cpuinfo_fp = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo_fp) {
        perror("detect_cpu_features: Failed to open /proc/cpuinfo");
        return -1;
    }

    char line_buffer[1024];
    int current_max_processor_id = -1; // To count unique processors
    bool flags_line_processed = false;

    while (fgets(line_buffer, sizeof(line_buffer), cpuinfo_fp)) {
        // Count cores based on unique "processor" lines or "cpu cores" line
        if (strncmp(line_buffer, "processor", strlen("processor")) == 0) {
            int processor_id = -1;
            if (sscanf(line_buffer, "processor\t: %d", &processor_id) == 1) {
                if (processor_id > current_max_processor_id) {
                    current_max_processor_id = processor_id;
                }
            }
        }
        // A more direct way for core count if available
        // else if (strncmp(line_buffer, "cpu cores", strlen("cpu cores")) == 0) {
        //    sscanf(line_buffer, "cpu cores\t: %d", &features->num_cores);
        // }


        // Process the 'flags' line only once (they should be the same for all cores of the same CPU)
        if (!flags_line_processed && strncmp(line_buffer, "flags", strlen("flags")) == 0) {
            char *flags_data = strchr(line_buffer, ':');
            if (flags_data && *(flags_data + 1) != '\0') {
                flags_data++; // Move past ':'
                // Trim leading space if any
                while (*flags_data == ' ' || *flags_data == '\t') flags_data++;

                strncpy(features->flags_line, flags_data, MAX_CPU_FEATURE_STR - 1);
                features->flags_line[MAX_CPU_FEATURE_STR - 1] = '\0';
                // Remove trailing newline if present
                char *newline = strrchr(features->flags_line, '\n');
                if (newline) *newline = '\0';


                // Check for specific features
                features->has_sse = check_flag(features->flags_line, "sse");
                features->has_sse2 = check_flag(features->flags_line, "sse2");
                features->has_avx = check_flag(features->flags_line, "avx");
                features->has_avx2 = check_flag(features->flags_line, "avx2");
                // Add more checks here if needed
            }
            flags_line_processed = true; // Process flags only from the first core entry
        }

        // Optimization: if we have processed flags and found num_cores (if using direct "cpu cores" line), we could break.
        // However, iterating through all "processor" lines is more robust for num_cores.
    }

    fclose(cpuinfo_fp);

    features->num_cores = current_max_processor_id + 1;
    if (features->num_cores == 0 && current_max_processor_id == -1) {
        // Fallback or error if no "processor" lines found or parsing failed
        // This might happen on non-Linux or unusual /proc/cpuinfo formats
        // For now, we'll leave num_cores as 0 if no processor lines are found.
        // A more robust solution might try sysconf(_SC_NPROCESSORS_ONLN) as a fallback.
        features->num_cores = 1; // Assume at least 1 core if parsing fails badly
    }


    // If flags_line is still empty, it means "flags" line was not found or not parsed.
    if (strlen(features->flags_line) == 0) {
        // This indicates a parsing issue or unexpected /proc/cpuinfo format.
        // Depending on strictness, could return an error code here.
        // For now, features will remain false.
    }

    return 0;
}

void print_cpu_features(const CpuFeatures *features) {
    if (!features) {
        printf("CPU Features struct is NULL.\n");
        return;
    }

    printf("--- CPU Features ---\n");
    printf("Number of Cores: %d\n", features->num_cores);
    printf("SSE Support:     %s\n", features->has_sse ? "Yes" : "No");
    printf("SSE2 Support:    %s\n", features->has_sse2 ? "Yes" : "No");
    printf("AVX Support:     %s\n", features->has_avx ? "Yes" : "No");
    printf("AVX2 Support:    %s\n", features->has_avx2 ? "Yes" : "No");
    // printf("Raw Flags Line:  %s\n", features->flags_line); // Can be very long
    printf("--------------------\n");
}
