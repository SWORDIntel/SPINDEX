file(GLOB CORE_SOURCES "*.c")
list(FILTER CORE_SOURCES EXCLUDE REGEX "main\\.c")
list(FILTER CORE_SOURCES EXCLUDE REGEX "test_.*\\.c")

add_library(spindex_core STATIC ${CORE_SOURCES})

target_link_libraries(spindex_core PUBLIC
    Threads::Threads
    CURL::libcurl
    Jansson::Jansson
    GEOS::geos_c # Assuming geos_c, might need adjustment
)

add_executable(spindex_app main.c)
target_link_libraries(spindex_app PRIVATE spindex_core spindex_aegis)
