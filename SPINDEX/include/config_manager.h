#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "config.h" // Defines SpindexConfig

/**
 * @brief Parses a configuration file and populates the SpindexConfig struct.
 *
 * Reads key-value pairs from the specified file.
 * Lines starting with '#' or ';' are treated as comments and ignored.
 * Empty lines are also ignored.
 * Expected format: key = value
 *
 * @param filepath Path to the configuration file.
 * @param config Pointer to a SpindexConfig struct to be filled.
 *               The struct should be initialized with default values by the caller
 *               before passing it to this function, as not all settings might
 *               be present in the config file.
 * @return 0 on success, -1 if the file cannot be opened,
 *         -2 for parsing errors (e.g., malformed lines).
 *         Partial success is possible (some values loaded before an error).
 */
int parse_config_file(const char *filepath, SpindexConfig *config);

/**
 * @brief Frees any dynamically allocated resources within the SpindexConfig struct.
 *
 * For the current SpindexConfig definition, all members are fixed-size arrays
 * or simple types, so this function might not do much. However, if string
 * members were dynamically allocated (e.g., char*), this function would free them.
 *
 * @param config Pointer to the SpindexConfig struct whose resources might be freed.
 */
void free_config_resources(SpindexConfig *config);

/**
 * @brief Prints the current configuration to stdout.
 *
 * Useful for debugging purposes.
 *
 * @param config Pointer to the SpindexConfig struct to be printed.
 */
void print_config(const SpindexConfig *config);

/**
 * @brief Sets default values for the SpindexConfig struct.
 *
 * This should be called before parsing the config file to ensure all
 * parameters have a sane default in case they are not specified in the file.
 *
 * @param config Pointer to the SpindexConfig struct to be initialized.
 */
void set_default_config(SpindexConfig *config);


#endif // CONFIG_MANAGER_H
