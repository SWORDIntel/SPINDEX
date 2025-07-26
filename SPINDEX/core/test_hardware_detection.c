#include "hardware_detection.h" // To test this module
#include <stdio.h> // For printf
#include <assert.h>

void test_detect_cpu_features() {
    printf("Running test: test_detect_cpu_features\n");
    CpuFeatures features;
    int result = detect_cpu_features(&features);
    assert(result == 0);
    print_cpu_features(&features);
    printf("Test passed\n");
}

void test_detect_meteor_lake_and_npu() {
    printf("Running test: test_detect_meteor_lake_and_npu\n");
    CpuFeatures features;
    int result = detect_cpu_features(&features);
    assert(result == 0);

    // This test will only pass on a Meteor Lake machine with an NPU.
    // On other machines, it will check that the fields are false.
    if (features.is_meteor_lake) {
        assert(features.has_npu == true);
    } else {
        assert(features.has_npu == false);
    }

    printf("Test passed\n");
}

int main() {
    test_detect_cpu_features();
    test_detect_meteor_lake_and_npu();
    // Add calls to more test functions here as they are created
    return 0;
}
