// File: SPINDEX/include/avx_search_params.h (Example)
// File: SPINDEX/include/avx_search_params.h
#ifndef AVX_SEARCH_PARAMS_H
#define AVX_SEARCH_PARAMS_H

#include "data_types.h" // For ProcessedDataChunk
#include <stdbool.h>   // For bool type

typedef struct {
    const ProcessedDataChunk* text_to_search;  // The main text body
    const ProcessedDataChunk* phrases_to_find; // Contains one or more phrases (e.g., newline-separated, or structured)
    bool case_sensitive;
    bool find_all_occurrences;
    int max_hits_per_doc; // Maximum number of occurrences to report per document/text_to_search if find_all_occurrences is true. 0 or negative means no limit.
} AvxSearchParams;

#endif // AVX_SEARCH_PARAMS_H
