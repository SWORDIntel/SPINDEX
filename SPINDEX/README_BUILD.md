# SPINDEX Project Build Instructions (CMake with Conan)

This document outlines the necessary steps to build the SPINDEX project using CMake, with C/C++ dependencies managed by Conan.

## 1. Prerequisites

Ensure you have the following software installed:

*   **CMake** (version 3.15 or higher)
*   **A C Compiler** (e.g., GCC, Clang) that supports C11.
*   **Make or Ninja** (or another build tool supported by CMake generators).
*   **Conan Package Manager:** (version 1.x or 2.x, conanfile.py is for Conan 2.x with `from conan import ConanFile`)
    *   Install Conan by following the official documentation (e.g., `pip install conan`).
    *   Ensure your Conan profile is configured correctly (e.g., C++ standard, compiler). If you encounter issues, you might need to create or update your default Conan profile (e.g., `conan profile detect`).
*   **Optional: Intel OpenVINO Toolkit Conan Package Configuration**
    *   The `openvino` Conan package is specified in `conanfile.py`. This package might not be available on Conan Center by default.
    *   You may need to add Intel's Conan remote:
        ```sh
        conan remote add intel https://intel.github.io/openvino-conan/index.json
        ```
    *   Consult the `conanfile.py` and OpenVINO documentation for the exact Conan package name and version if issues arise. The build will proceed without OpenVINO features if the package cannot be resolved and `USE_OPENVINO` is ON but the package is not found by CMake.

## 2. Build Steps

1.  **Create a build directory:**
    It's highly recommended to perform an out-of-source build. From the `SPINDEX` root directory:
    ```sh
    mkdir build_cmake
    # cd build_cmake # Subsequent commands can be run from SPINDEX root or build_cmake
    ```
    *(Using `build_cmake` to distinguish from any potential legacy `build` directory from Makefiles).*

2.  **Install Dependencies with Conan:**
    From the `SPINDEX` root directory, run Conan to install dependencies. This command will download/build the required libraries and generate necessary CMake integration files.
    ```sh
    conan install . --output-folder=build_cmake --build=missing --settings:host build_type=Debug # Or Release
    ```
    *   Replace `Debug` with `Release` for a release build. This should match the `CMAKE_BUILD_TYPE` you intend to use.
    *   The `--output-folder=build_cmake` directs Conan to place its generated files (like `conan_toolchain.cmake`) into `build_cmake/generators/`.

3.  **Configure with CMake:**
    From the `SPINDEX` root directory (or you can `cd build_cmake` and adjust paths):
    ```sh
    cmake -S . -B build_cmake -DCMAKE_TOOLCHAIN_FILE=build_cmake/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug # Or Release
    ```
    *   Ensure `CMAKE_BUILD_TYPE` matches the `build_type` used in `conan install`.
    *   The `CMAKE_TOOLCHAIN_FILE` tells CMake to use the settings and dependency information provided by Conan.
    *   **OpenVINO Support:** By default, the build attempts to use OpenVINO if the Conan package is found (`USE_OPENVINO=ON`). To explicitly disable OpenVINO features even if the package is available:
        ```sh
        cmake -S . -B build_cmake -DCMAKE_TOOLCHAIN_FILE=build_cmake/generators/conan_toolchain.cmake -DUSE_OPENVINO=OFF ...
        ```
    *   **AVX-512 Plugin:** The AVX-512 plugin is built by default if the source file exists. To disable it:
        ```sh
        cmake -S . -B build_cmake -DCMAKE_TOOLCHAIN_FILE=build_cmake/generators/conan_toolchain.cmake -DBUILD_AVX512_PLUGIN=OFF ...
        ```
    *   Review CMake output carefully for any errors or warnings. It should indicate that dependencies like Jansson, CURL, GEOS, and potentially OpenVINO are found via Conan.

4.  **Compile the Project:**
    After successful configuration, compile the project from the `SPINDEX` root directory:
    ```sh
    cmake --build build_cmake
    ```
    Or, navigate to `build_cmake` and run your build tool:
    ```sh
    cd build_cmake
    make -j$(nproc) # Or ninja
    ```
    This will compile:
    *   The main application: `build_cmake/bin/spindex_app`
    *   Core library: `build_cmake/lib/libspindex_core_lib.a`
    *   Plugins: `build_cmake/lib/spindex/plugins/*.so`
    *   Test executables: `build_cmake/bin/test_*`

## 3. Running Tests

After a successful build, you can run the tests using CTest from your `build_cmake` directory:
```sh
cd build_cmake
ctest --verbose
```
Or, to see output from failing tests only:
```sh
ctest --output-on-failure
```
This will execute all defined tests.
*   Dependencies (Jansson, CURL, GEOS, OpenVINO, etc.) are provided by Conan. The CMake/Conan toolchain setup should ensure that executables and libraries can find these runtime dependencies.
*   Tests that load SPINDEX plugins (e.g., `test_ingestion_pipeline`) are configured by CMake to find plugins from the `build_cmake/lib/spindex/plugins/` directory via the `SPINDEX_PLUGIN_DIR` environment variable set during testing.
*   `test_openvino_classifier_hw` requires a correctly configured OpenVINO environment (libraries via Conan) and appropriate hardware.

## 4. Legacy Makefile

A legacy Makefile (`Makefile.legacy`) exists in the `SPINDEX/` directory. **This CMake and Conan based build system is now the primary way to build the project.** The legacy Makefile should not be used for new builds.
