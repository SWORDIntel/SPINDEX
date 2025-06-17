file(GLOB CORE_SOURCES "*.c")
# Filter out test_*.c files
list(FILTER CORE_SOURCES EXCLUDE REGEX "test_.*\\.c$")

# Create an object library for core source files (useful for tests too)
add_library(spindex_core_objects OBJECT ${CORE_SOURCES})
target_include_directories(spindex_core_objects PUBLIC
    ../include
    ${CMAKE_CURRENT_SOURCE_DIR} # For core-specific headers if any
    ${JANSSON_INCLUDE_DIRS} # For Jansson headers
    ${GEOS_INCLUDE_DIRS} # For GEOS headers
    ../plugins/aegis_modules # For nlp_ner_parser.h etc.
)

# Main application executable
add_executable(spindex_app ${CORE_SOURCES}) # main.c should be in CORE_SOURCES

target_include_directories(spindex_app PUBLIC
    ../include # For project-wide includes like data_types.h
    # No, elasticsearch_client.h is in ../include, not ../../include from core
    # And config_manager.h, plugin_interface.h are also in ../include
    ${CMAKE_CURRENT_SOURCE_DIR} # For core-specific headers if any (e.g. plugin_loader.h, hardware_detection.h if they exist in core/)
    ${JANSSON_INCLUDE_DIRS} # For Jansson headers
    ${GEOS_INCLUDE_DIRS} # For GEOS headers
    # Also add aegis_modules to spindex_app includes if its public headers are needed directly by main.c
    # However, ingestion_pipeline.h (which is in ../include) is the one including nlp_ner_parser.h
    # and ingestion_pipeline.c is part of spindex_app's sources.
    # So spindex_app also needs this include path.
    ../plugins/aegis_modules # For nlp_ner_parser.h etc.
)

target_link_libraries(spindex_app PUBLIC
    aegis_modules # Link against the library from plugins/aegis_modules
    ${JANSSON_LIBRARIES} # Link Jansson using pkg-config variables
    ${GEOS_LIBRARIES}    # Link GEOS using pkg-config variables
    CURL::libcurl    # CMake target for CURL
    Threads::Threads # CMake target for PThreads
    ${CMAKE_DL_LIBS} # For dlopen/dlsym
)

# The comment about elasticsearch_client.h include path:
# The target_include_directories already includes ../include.
# The original Makefile had:
# CORE_OBJS = main.o config_manager.o plugin_loader.o hardware_detection.o elasticsearch_client.o
# INC_DIRS  = -Iinclude -Icore -Iplugins/parsers -Iplugins/preprocessors -Iplugins/indexers -Iplugins/decompressors -Iplugins/aegis_modules
# elasticsearch_client.c includes "elasticsearch_client.h" and "config_manager.h"
# "elasticsearch_client.h" is in include/
# "config_manager.h" is in include/
# So, for elasticsearch_client.c (and other core files) to find these, ../include is correct.
# The CMAKE_CURRENT_SOURCE_DIR for core adds SPINDEX/core/ if there were core-specific headers there.

# Note on CORE_SOURCES and main.c:
# The current approach with `add_executable(spindex_app ${CORE_SOURCES})` assumes main.c
# is captured by `file(GLOB CORE_SOURCES "*.c")`. This is typical.
# If main.c were separate, one might do `add_executable(spindex_app main.c $<TARGET_OBJECTS:spindex_core_objects>)`
# or explicitly list sources. The current way is fine if main.c is like any other .c file in core/.

# No specific handling for elasticsearch_client.o needed unless it requires unique compile flags
# not covered by the global CMAKE_C_FLAGS. Assumed to be fine for now.
