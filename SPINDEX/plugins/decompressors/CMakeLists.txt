# SPINDEX/plugins/decompressors/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Gzip Decompressor Plugin
# Source: gzip_decompressor.c
# Depends on ZLIB (managed by Conan)
add_library(gzip_decompressor SHARED gzip_decompressor.c)
# Assuming Conan provides ZLIB::ZLIB or zlib::zlib. Standard is often ZLIB::ZLIB.
target_link_libraries(gzip_decompressor PRIVATE ZLIB::ZLIB)

# Zip Decompressor Plugin
# Source: zip_decompressor.c
# Depends on libzip (managed by Conan)
add_library(zip_decompressor SHARED zip_decompressor.c)
# Assuming Conan provides libzip::libzip.
target_link_libraries(zip_decompressor PRIVATE libzip::libzip)

# Zstd Decompressor Plugin
# Source: zstd_decompressor.c
# Depends on Zstd (libzstd) (managed by Conan)
add_library(zstd_decompressor SHARED zstd_decompressor.c)
# Assuming Conan provides Zstd::zstd or zstd::zstd.
target_link_libraries(zstd_decompressor PRIVATE Zstd::zstd)

# Common include directories for plugin_interface.h etc. are expected
# to be inherited from parent CMakeLists.txt or set globally.
# Output directory for .so files is handled by CMAKE_LIBRARY_OUTPUT_DIRECTORY
# set in the top-level SPINDEX/CMakeLists.txt.
