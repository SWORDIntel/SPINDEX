# SPINDEX Project Build Instructions

This document outlines the necessary steps to build the SPINDEX project using CMake.

## 1. Prerequisites

Ensure you have the following software installed:

*   **CMake** (version 3.15 or higher)
*   **A C Compiler** (e.g., GCC, Clang) that supports C11.
*   **Make or Ninja** (or another build tool supported by CMake generators).
*   **pkg-config** (for detecting dependencies like Jansson, GEOS).
*   **Conan Package Manager:**
    *   Conan is used to manage C/C++ dependencies for this project.
    *   Install Conan by following the official documentation (e.g., `pip install conan`).
*   **Development Libraries (Managed by Conan):**
    *   Jansson
    *   libcurl
    *   GEOS
    *   (pthreads is usually part of the C compiler/standard library)
*   **Optional: Intel OpenVINO Toolkit**
    *   If you need the `openvino_classifier` plugin or to run `test_openvino_classifier_hw`.
    *   Ensure the OpenVINO environment is initialized (e.g., by sourcing `setupvars.sh`) or that CMake can find it (e.g., by setting the `OpenVINO_DIR` environment variable or CMake variable).

## 2. Build Steps

**The canonical way to build this project is now via CMake, using Conan for dependency management.**

1.  **Create a build directory:**
    It's recommended to perform an out-of-source build. From the `SPINDEX` root directory:
    ```sh
    mkdir build
    ```

2.  **Install Dependencies with Conan:**
    From the `SPINDEX` root directory, run Conan to install dependencies. This command will download the required libraries and generate necessary files for CMake.
    ```sh
    conan install . --output-folder=build --build=missing
    ```
    Ensure your Conan profile is configured correctly (e.g., C++ standard, compiler). If you encounter issues, you might need to create or update your default Conan profile (`conan profile detect`).

3.  **Configure with CMake:**
    From the `SPINDEX` root directory, run CMake to configure the project, pointing it to the Conan-generated toolchain file.
    ```sh
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake
    ```
    *   If OpenVINO is installed in a non-standard path and not managed by Conan, you might still need to specify its location:
        ```sh
        cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake -DOpenVINO_DIR=/path/to/your/openvino
        ```
    *   Review CMake output for any errors or warnings.

4.  **Compile the Project:**
    After configuration, navigate to the build directory and compile.
    ```sh
    cd build
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
    *   Executables (application and tests) will be placed in `build/bin/` (or a similar path depending on your CMake configuration).
    *   Libraries (`.a` and `.so` files) will be placed in `build/lib/` (or a similar path).

## 3. Running Tests

After a successful build, you can run the tests using CTest from the build directory:

```sh
ctest --verbose
```
This will execute all defined tests. Some tests might require specific conditions:
*   `test_ingestion_pipeline` and `test_elasticsearch_indexing` require plugins to be loaded. The CMake build system stages these plugins into `build/plugins_for_test/`, and the tests are configured to run from `build/` (their CWD for CTest) to find them via the relative path `"plugins_for_test"`.
*   `test_openvino_classifier_hw` is designed for specific hardware and requires OpenVINO. It might be skipped or fail if these conditions aren't met.

## 4. Legacy Makefile

A legacy Makefile might still be present in `SPINDEX/Makefile`. **The canonical way to build this project is now via CMake with Conan for dependency management.** The Makefile is considered legacy and will be renamed.
