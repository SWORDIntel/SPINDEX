#ifndef NLP_NER_PARSER_H
#define NLP_NER_PARSER_H

#include "config_manager.h" // For Config struct
#include "data_types.h"     // For AEGIS_PROCESSING_STATUS and MAX_PATH_LEN

typedef struct {
    char script_path[MAX_PATH_LEN];
    char python_executable[MAX_PATH_LEN];
    int enabled;
} NlpNerParserConfig;

/**
 * @brief Loads the NLP/NER parser configuration from the global config.
 *
 * @param global_config Pointer to the global Config struct from config_manager.
 * @param ner_config Pointer to NlpNerParserConfig to be populated.
 * @return 0 on success, -1 on failure (e.g., required config keys missing).
 */
int load_nlp_ner_parser_config(const SpindexConfig* global_config, NlpNerParserConfig* ner_config);

/**
 * @brief Extracts location names from unstructured text using an external NER script.
 *
 * @param text The input unstructured text (UTF-8 encoded).
 * @param ner_config The configuration for the NLP/NER parser.
 * @param extracted_locations_out Pointer to a char** that will be allocated and filled
 *                                with extracted location names. The caller must free this
 *                                array and its contents using free_extracted_locations().
 * @param num_locations_out Pointer to an int where the number of extracted locations will be stored.
 * @return AEGIS_PROCESSING_STATUS indicating success, failure, or if NER is disabled.
 *         Returns AEGIS_PROCESSING_NLP_NER_DISABLED if ner_config->enabled is false.
 */
AEGIS_PROCESSING_STATUS extract_locations_from_text(
    const char* text,
    const NlpNerParserConfig* ner_config,
    char*** extracted_locations_out,
    int* num_locations_out
);

/**
 * @brief Frees the memory allocated for the extracted locations array and its strings.
 *
 * @param locations_array The array of location strings to free.
 * @param num_locations The number of strings in the array.
 */
void free_extracted_locations(char** locations_array, int num_locations);

#endif // NLP_NER_PARSER_H
