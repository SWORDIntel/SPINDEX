#include "config_manager.h" // To test this module
#include <stdio.h>

int main() {
    SpindexConfig config;
    const char *config_filepath = "/app/test_spindex.conf"; // Relative to where the test will be run or absolute

    printf("--- Starting Config Manager Test ---\n");

    // 1. Set default configuration
    printf("Setting default configuration...\n");
    set_default_config(&config);
    print_config(&config);

    // 2. Parse the configuration file
    printf("\nParsing configuration file: %s...\n", config_filepath);
    int result = parse_config_file(config_filepath, &config);

    if (result == 0) {
        printf("Successfully parsed config file.\n");
    } else if (result == -1) {
        fprintf(stderr, "Error: Could not open or read config file: %s\n", config_filepath);
        // Continue to print whatever config we have (defaults, or partial if that was supported)
    } else if (result == -2) {
        fprintf(stderr, "Error: Parsing errors in config file: %s\n", config_filepath);
        // Continue to print
    }

    // 3. Print the final configuration
    printf("\nFinal configuration after parsing:\n");
    print_config(&config);

    // 4. Free configuration resources (mostly a placeholder for now)
    printf("\nCalling free_config_resources...\n");
    free_config_resources(&config);
    printf("free_config_resources called.\n");

    printf("\n--- Config Manager Test Complete ---\n");

    return 0;
}
