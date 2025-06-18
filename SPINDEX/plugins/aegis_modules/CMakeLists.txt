#file(GLOB AEGIS_MODULE_SOURCES "*.c" "*.h") # Include .h for IDEs, won't be compiled directly
# Filter out .h files from sources to be compiled
#list(FILTER AEGIS_MODULE_SOURCES EXCLUDE REGEX "\\.h$")

set(AEGIS_MODULE_SOURCES
    complex_geometry_handler.c
    geojson_processor.c
    geometric_validator.c
    nlp_ner_parser.c
    )

# Create a static library for AEGIS modules
add_library(aegis_modules STATIC ${AEGIS_MODULE_SOURCES})

target_include_directories(aegis_modules PUBLIC
    ../../include # To access common headers like data_types.h, config_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR} # For aegis_modules specific headers
)

# If AEGIS modules directly use Jansson or GEOS, link them here.
# Example:
# target_link_libraries(aegis_modules PUBLIC Jansson::Jansson)
# target_link_libraries(aegis_modules PUBLIC GEOS::GEOS)
# From Makefile, it seems they don't directly link these, but rely on main app linking.
# For a static library, PUBLIC/INTERFACE link dependencies are usually not needed
# if the library itself doesn't use them directly in its *public headers*.
# If aegis_modules .c files use jansson, then it might need:
# target_link_libraries(aegis_modules PRIVATE Jansson::Jansson)
# However, typically these are passed down from the main executable.
# The current setup, where spindex_app links Jansson, GEOS, etc., should be sufficient
# as long as aegis_modules doesn't expose their types in its headers.
# Given the C structure, this is likely fine.
