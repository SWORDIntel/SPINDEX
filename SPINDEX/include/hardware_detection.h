#ifndef HARDWARE_DETECTION_H
#define HARDWARE_DETECTION_H

#include <stdbool.h> // For bool type

#define MAX_CPU_FEATURE_STR 512 // Max length for storing the 'flags' line

/**
 * @brief Structure to hold detected CPU features.
 */
typedef struct {
    int num_cores;
    bool has_sse;
    bool has_sse2;
    bool has_avx;
    bool has_avx2;
    bool has_avx512f;    // AVX-512 Foundation
    bool has_avx512dq;   // AVX-512 Doubleword and Quadword Instructions
    bool has_avx512bw;   // AVX-512 Byte and Word Instructions
    bool has_avx512vl;   // AVX-512 Vector Length Extensions
    // Add more features as needed, e.g.:
    // bool has_neon; // For ARM
    // bool has_aes_ni;
    char flags_line[MAX_CPU_FEATURE_STR]; // Store the raw flags line for reference
} CpuFeatures;

/**
 * @brief Detects CPU features by parsing system information (e.g., /proc/cpuinfo on Linux).
 *
 * The caller should allocate a CpuFeatures struct and pass a pointer to it.
 * This function will attempt to populate the fields of the struct.
 *
 * @param features Pointer to a CpuFeatures struct to be filled.
 * @return 0 on success, -1 if system information cannot be accessed or parsed.
 */
int detect_cpu_features(CpuFeatures *features);

/**
 * @brief Prints the detected CPU features to stdout.
 *
 * @param features Pointer to a CpuFeatures struct containing the detected features.
 */
void print_cpu_features(const CpuFeatures *features);

/**
 * @brief Initializes the CpuFeatures struct to default (false/zero) values.
 * @param features Pointer to a CpuFeatures struct to be initialized.
 */
void init_cpu_features(CpuFeatures *features);


#endif // HARDWARE_DETECTION_H
