#include "geocoding_service.h"
#include "utils.h" // For LOG_INFO, LOG_WARN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stub implementation for geocode_location
int geocode_location(const char *location_text, GeocodeResult *result) {
    if (!location_text || !result) {
        LOG_WARN("geocode_location: NULL input parameters.");
        return -1;
    }

    // Placeholder behavior: Log the request and return dummy data
    LOG_INFO("Geocoding (stub): Request to geocode location: \"%s\"", location_text);

    // Simulate a successful geocoding with dummy coordinates
    result->latitude = 0.0; // Dummy latitude
    result->longitude = 0.0; // Dummy longitude

    // Allocate and copy a dummy address detail
    const char* dummy_detail = "Placeholder Address - Geocoding Not Implemented";
    result->address_details = strdup(dummy_detail);
    if (!result->address_details) {
        LOG_WARN("geocode_location: Failed to allocate memory for address_details.");
        // Even if allocation fails, we can still return 0 as "success" for the stub,
        // but the details will be missing. Or choose to return -1.
        // For a stub, let's still call it a "successful" call to the stub.
    }

    LOG_INFO("Geocoding (stub): Successfully processed \"%s\". Returning dummy coordinates.", location_text);
    return 0; // Simulate success
}

void free_geocode_result(GeocodeResult *result) {
    if (result && result->address_details) {
        free(result->address_details);
        result->address_details = NULL;
    }
}

// Example of how a batch processing function might be stubbed, if needed later.
// int process_locations_for_geocoding(char** locations, int num_locations, GeocodeResult** results_array_ptr) {
//     if (!locations || num_locations <= 0 || !results_array_ptr) {
//         LOG_WARN("process_locations_for_geocoding: Invalid input.");
//         return -1;
//     }
//     LOG_INFO("Geocoding (stub): Request to geocode %d locations.", num_locations);
//
//     *results_array_ptr = (GeocodeResult*)malloc(sizeof(GeocodeResult) * num_locations);
//     if (!(*results_array_ptr)) {
//         LOG_ERROR("process_locations_for_geocoding: Failed to allocate memory for results array.");
//         return -1;
//     }
//
//     for (int i = 0; i < num_locations; ++i) {
//         // Call the single geocode_location stub for each
//         if (geocode_location(locations[i], &((*results_array_ptr)[i])) != 0) {
//             LOG_WARN("Geocoding (stub): Failed to geocode location (batch): %s", locations[i]);
//             // Handle error for individual item, e.g., set to known invalid values
//             (*results_array_ptr)[i].latitude = -999.0;
//             (*results_array_ptr)[i].longitude = -999.0;
//             (*results_array_ptr)[i].address_details = NULL;
//         }
//     }
//     return num_locations; // Or return number successfully geocoded
// }

// void free_geocode_results_array(GeocodeResult* results_array, int num_results) {
//     if (results_array) {
//         for (int i = 0; i < num_results; ++i) {
//             free_geocode_result(&results_array[i]);
//         }
//         free(results_array);
//     }
// }
