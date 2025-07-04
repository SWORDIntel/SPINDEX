# SPINDEX Project Build Instructions (CMake)

This document outlines the necessary steps to build the SPINDEX project using CMake.

## 1. Prerequisites

Ensure you have the following software installed:

*   **CMake** (version 3.15 or higher)
*   **A C Compiler** (e.g., GCC, Clang) that supports C11.
*   **Make or Ninja** (or another build tool supported by CMake generators).
*   **pkg-config** (recommended, helps CMake find some dependencies).
*   **Development Libraries:**
    *   **Jansson:** For JSON processing.
        *   E.g., on Debian/Ubuntu: `sudo apt install libjansson-dev`
        *   E.g., on Fedora: `sudo dnf install jansson-devel`
        *   E.g., on macOS (Homebrew): `brew install jansson`
    *   **libcurl:** For network operations (e.g., Elasticsearch client).
        *   E.g., on Debian/Ubuntu: `sudo apt install libcurl4-openssl-dev` or `libcurl-gnutls-dev`
        *   E.g., on Fedora: `sudo dnf install libcurl-devel`
        *   E.g., on macOS (Homebrew): `brew install curl`
    *   **GEOS:** For geometric operations.
        *   E.g., on Debian/Ubuntu: `sudo apt install libgeos-dev` (ensure `geos-config` is available)
        *   E.g., on Fedora: `sudo dnf install geos-devel`
        *   E.g., on macOS (Homebrew): `brew install geos`
    *   **pthreads:** Usually part of the C compiler/standard library; no separate installation typically needed.
*   **Optional: Intel OpenVINO Toolkit**
    *   Required if you need the `openvino_classifier` plugin or want to run the `test_openvino_classifier_hw`.
    *   Install the toolkit according to Intel's documentation.
    *   Ensure the OpenVINO environment is initialized (e.g., by sourcing `setupvars.sh` from the OpenVINO installation directory) so that the `INTEL_OPENVINO_DIR` environment variable is set, OR
    *   You can specify the path during CMake configuration using `-DOpenVINO_DIR=/path/to/your/openvino`.

## 2. Build Steps

1.  **Create a build directory:**
    It's highly recommended to perform an out-of-source build. From the `SPINDEX` root directory:
    ```sh
    mkdir build_cmake
    cd build_cmake
    ```
    *(Using `build_cmake` to distinguish from any potential legacy `build` directory from Makefiles).*

2.  **Configure with CMake:**
    From your `build_cmake` directory, run CMake to configure the project:
    ```sh
    cmake ..
    ```
    *   **OpenVINO Path:** If `INTEL_OPENVINO_DIR` is not set and OpenVINO is installed in a non-standard location, specify its path:
        ```sh
        cmake .. -DOpenVINO_DIR=/path/to/your/openvino
        ```
    *   **Build Type:** To specify a build type (e.g., Release), use:
        ```sh
        cmake .. -DCMAKE_BUILD_TYPE=Release
        ```
        (Default is Debug if not specified).
    *   **AVX-512 Plugin:** The AVX-512 plugin is built by default if the source file exists. To disable it:
        ```sh
        cmake .. -DBUILD_AVX512_PLUGIN=OFF
        ```
    *   Review CMake output carefully for any errors or warnings, especially regarding found or missing dependencies.

3.  **Compile the Project:**
    After successful configuration, still within your `build_cmake` directory, compile the project:
    ```sh
    cmake --build .
    ```
    Or, if using Make directly:
    ```sh
    make -j$(nproc) # Builds in parallel using all available processors
    ```
    Or, if using Ninja:
    ```sh
    ninja
    ```
    This will compile:
    *   The main application: `build_cmake/bin/spindex_app`
    *   Core library: `build_cmake/lib/libspindex_core_lib.a`
    *   Plugins: `build_cmake/lib/spindex/plugins/*.so`
    *   Test executables: `build_cmake/bin/test_*`

## 3. Running Tests

After a successful build, you can run the tests using CTest from your `build_cmake` directory:

```sh
ctest --verbose
```
Or, to see output from failing tests only:
```sh
ctest --output-on-failure
```
This will execute all defined tests.
*   Tests that load plugins (e.g., `test_ingestion_pipeline`, `test_plugin_loader`, `test_elasticsearch_indexing`, `test_openvino_classifier_hw`) are configured by CMake to find plugins from the `build_cmake/lib/spindex/plugins/` directory via the `SPINDEX_PLUGIN_DIR` environment variable set during testing.
*   `test_openvino_classifier_hw` is designed for specific hardware and requires a correctly configured OpenVINO environment. It might be skipped during the build if OpenVINO is not found, or fail at runtime if conditions aren't met.

## 4. Legacy Makefile

A legacy Makefile (`Makefile.legacy`) exists in the `SPINDEX/` directory. **This CMake build system is now the primary way to build the project.** The legacy Makefile should not be used for new builds.
