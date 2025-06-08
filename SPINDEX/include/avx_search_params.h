// File: SPINDEX/include/avx_search_params.h (Example)
#ifndef AVX_SEARCH_PARAMS_H
#define AVX_SEARCH_PARAMS_H

#include "data_types.h" // For ProcessedDataChunk

typedef struct {
    const ProcessedDataChunk* text_to_search;  // The main text body
    const ProcessedDataChunk* phrases_to_find; // Contains one or more phrases (e.g., newline-separated, or structured)
    // Future options:
    bool case_sensitive;
    bool find_all_occurrences;
    // int max_hits_per_doc;
} AvxSearchParams;

#endif // AVX_SEARCH_PARAMS_H
