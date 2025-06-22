#ifndef GEOCODING_SERVICE_H
#define GEOCODING_SERVICE_H

// Placeholder for actual geocoding results
typedef struct {
    double latitude;
    double longitude;
    char* address_details; // Could store formatted address or other info
} GeocodeResult;

/**
 * @brief Geocodes a single location string.
 *
 * @param location_text The location string to geocode (e.g., "1600 Amphitheatre Parkway, Mountain View, CA").
 * @param result A pointer to a GeocodeResult struct to be populated.
 * @return 0 on success, -1 on failure (e.g., location not found, service error).
 *
 * @note This is currently a placeholder implementation.
 */
int geocode_location(const char *location_text, GeocodeResult *result);

/**
 * @brief Frees resources associated with a GeocodeResult, particularly dynamically allocated strings.
 *
 * @param result Pointer to the GeocodeResult to free.
 */
void free_geocode_result(GeocodeResult *result);

// If a function to process a list of locations is desired:
// int process_locations_for_geocoding(char** locations, int num_locations, GeocodeResult** results_array);
// void free_geocode_results_array(GeocodeResult* results_array, int num_results);

#endif // GEOCODING_SERVICE_H
