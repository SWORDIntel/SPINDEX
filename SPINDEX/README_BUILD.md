# SPINDEX Project Build Instructions

This document outlines the necessary steps to build the SPINDEX project using CMake.

## 1. Prerequisites

Ensure you have the following software installed:

*   **CMake** (version 3.15 or higher)
*   **A C Compiler** (e.g., GCC, Clang) that supports C11.
*   **Make or Ninja** (or another build tool supported by CMake generators).
*   **pkg-config** (for detecting dependencies like Jansson, GEOS).
*   **Development Libraries:**
    *   Jansson (libjansson-dev or equivalent)
    *   libcurl (libcurl4-openssl-dev or equivalent)
    *   GEOS (libgeos-dev or equivalent, specifically geos-c)
    *   pthreads (usually part of the C compiler/standard library)
*   **Optional: Intel OpenVINO Toolkit**
    *   If you need the `openvino_classifier` plugin or to run `test_openvino_classifier_hw`.
    *   Ensure the OpenVINO environment is initialized (e.g., by sourcing `setupvars.sh`) or that CMake can find it (e.g., by setting the `OpenVINO_DIR` environment variable or CMake variable).

## 2. Build Steps

1.  **Create a build directory:**
    It's recommended to perform an out-of-source build.
    ```sh
    mkdir build
    cd build
    ```

2.  **Configure with CMake:**
    From the build directory, run CMake to configure the project. The source directory for SPINDEX is assumed to be `../SPINDEX` if your current directory is `build` at the same level as `SPINDEX`. If your `build` directory is inside `SPINDEX`, it would be `cmake ..`. Adjust accordingly. For this example, we assume `build` is a sibling of `SPINDEX`'s top-level source directory. More typically, if you clone `SPINDEX` and `cd SPINDEX`, you'd do `mkdir build && cd build && cmake ..`. The path to sources should be adjusted based on where the `SPINDEX` top-level `CMakeLists.txt` is relative to your build directory.

    Example assuming `build` is inside the top-level `SPINDEX` directory:
    ```sh
    # From SPINDEX/
    mkdir build
    cd build
    cmake ..
    ```
    *   If OpenVINO is installed in a non-standard path, you might need to specify its location:
        ```sh
        cmake -DOpenVINO_DIR=/path/to/your/openvino ..
        ```
    *   Review CMake output for any errors or warnings, especially regarding dependency detection.

3.  **Compile the Project:**
    Use the build tool specified by your CMake generator (e.g., Make or Ninja).
    ```sh
    cmake --build .
    ```
    Or, if using Make:
    ```sh
    make -j$(nproc) # Builds in parallel
    ```
    Or, if using Ninja:
    ```sh
    ninja
    ```
    This will compile the main `spindex_app` executable, all plugins (`.so` files), and all test executables.
    *   Executables (application and tests) will be placed in `build/bin/`.
    *   Libraries (`.a` and `.so` files) will be placed in `build/lib/`.

## 3. Running Tests

After a successful build, you can run the tests using CTest from the build directory:

```sh
ctest --verbose
```
This will execute all defined tests. Some tests might require specific conditions:
*   `test_ingestion_pipeline` and `test_elasticsearch_indexing` require plugins to be loaded. The CMake build system stages these plugins into `build/plugins_for_test/`, and the tests are configured to run from `build/` (their CWD for CTest) to find them via the relative path `"plugins_for_test"`.
*   `test_openvino_classifier_hw` is designed for specific hardware and requires OpenVINO. It might be skipped or fail if these conditions aren't met.

## 4. Legacy Makefile

A legacy Makefile might still be present in `SPINDEX/Makefile`. The canonical way to build this project is now via CMake.
