#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *config_file_path = argv[1];

    printf("Configuration file path: %s\n", config_file_path);

    // Further processing of the config file would go here.

    return EXIT_SUCCESS;
}
