#include "hardware_detection.h" // To test this module
#include <stdio.h> // For printf

int main() {
    printf("--- Starting Hardware Detection Test ---\n");

    CpuFeatures features;

    // Initialize the structure
    init_cpu_features(&features);
    printf("Initial (default) CPU features:\n");
    print_cpu_features(&features);

    // Detect features
    printf("\nDetecting CPU features...\n");
    if (detect_cpu_features(&features) == 0) {
        printf("Successfully detected CPU features.\n");
    } else {
        fprintf(stderr, "Failed to detect CPU features. Output may be defaults or partial.\n");
    }

    // Print detected features
    printf("\nDetected CPU features:\n");
    print_cpu_features(&features);

    // Optionally print the raw flags line if it's not too long or for detailed debugging
    // if (strlen(features.flags_line) > 0 && strlen(features.flags_line) < 200) { // Avoid printing extremely long lines
    //    printf("Raw flags: %s\n", features.flags_line);
    // }


    printf("\n--- Hardware Detection Test Complete ---\n");

    return 0;
}
