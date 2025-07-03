from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps
# from conan.tools.layout import cmake_layout # For standard layout - Not used, layout is manual

class SpindexConan(ConanFile):
    name = "spindex"
    version = "0.1.0" # Example version for SPINDEX itself
    settings = "os", "compiler", "build_type", "arch"
    # exports_sources are files copied from source to build folder for out-of-source builds
    # For an application, this might include all sources needed to build it.
    # If building in-source or if CMake handles finding sources correctly relative to
    # the CMakeLists.txt, this might not be strictly necessary or could be more minimal.
    # For now, let's list the key CMake file and source directories.
    exports_sources = "CMakeLists.txt", "core/*", "include/*", "plugins/*", "tests/*"

    def requirements(self):
        self.requires("libcurl/8.4.0")
        self.requires("jansson/2.14")
        self.requires("geos/3.12.0") # Updated GEOS to a more recent common version
        # For OpenVINO, users will likely need to configure a specific remote
        # (e.g., Intel's Conan repository) or have a local recipe.
        # This is a placeholder; the exact package name and version will depend
        # on how OpenVINO is made available to Conan.
        self.requires("openvino/2023.2.0", transitive_headers=True) # transitive_headers might be useful for OpenVINO
        self.requires("libzip/1.10.1")
        self.requires("zlib/1.3")
        self.requires("zstd/1.5.5")

    def layout(self):
        # Defines standard project layout.
        # self.source_folder will be the directory containing conanfile.py.
        # self.build_folder will be 'build/<build_type>' if using cmake_layout presets.
        # For simplicity here, let's define a flat build folder.
        # The conan_toolchain.cmake and other generated files will go into
        # self.generators_folder, which defaults to build_folder/conan.
        # cmake_layout(self, src_folder=".", build_folder="build/conan")
        # This means:
        # - Source root: SPINDEX/ (where conanfile.py is)
        # - Build root for conan install & build: SPINDEX/build/conan/
        # - CMake generated files (toolchain, deps): SPINDEX/build/conan/ (or SPINDEX/build/conan/generators/ based on exact Conan version behavior)
        # User would typically run:
        # cd SPINDEX
        # conan install . --output-folder=build/conan --build=missing
        # cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake
        # cmake --build build
        # This needs careful coordination with README_BUILD.md later.
        # A simpler approach for user might be to have conan install generate into current build dir.
        # self.folders.build = "build" # Let conan install populate the user's chosen build dir.
        # self.folders.generators = "." # Generate in current build dir (e.g. build/conan_toolchain.cmake)
        # This requires conan install to be run from the build directory.

        # Let's use the standard cmake_layout which is good for out-of-source builds
        # and works well with modern CMake integration.
        # It will set source_folder=".", build_folder="build" (or build/Debug etc.)
        # and generators will be in build_folder/generators.
        # The user workflow:
        # cd SPINDEX
        # conan install . --output-folder=build --build=missing
        # cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake
        # cmake --build build
        # This is a common and clean setup.
        # If `conan install . --output-folder=build` is used, Conan will place
        # generated files (like conan_toolchain.cmake) into "build/generators/" by default
        # when using CMakeToolchain and CMakeDeps.
        # Explicitly setting self.folders.generators ensures this behavior,
        # especially for local conan commands like `conan build .` if used.
        # self.folders.build typically refers to where the project itself would be built
        # if `conan build .` was invoked.
        # The README directs users to an out-of-source build using `cmake -S . -B build`.
        # So, the key is that CMakeToolchain/CMakeDeps output to a predictable subfolder
        # of the directory specified in `conan install --output-folder`.
        # "<output_folder>/generators" is the default for these generators.

        # To align with README's `conan install . --output-folder=build` and
        # `CMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake`:
        self.folders.source = "." # Default, location of conanfile.py
        # self.folders.build can be set if you use `conan build .` command.
        # For the user flow in README_BUILD.md, this particular self.folders.build
        # is less critical than self.folders.generators relative to the --output-folder.
        # Let's set it to "build" for consistency if `conan build .` were used.
        self.folders.build = "build"
        # This ensures that if `conan install .` is run (no --output-folder),
        # files go to "build/generators". If `conan install . --output-folder=mybuild` is run,
        # they go to "mybuild/generators". This matches the README.
        self.folders.generators = "generators" # Relative to the installation folder (e.g. 'build' or what --output-folder specifies)


    def generate(self):
        # Generates CMakeToolchain file (conan_toolchain.cmake in generators_folder)
        tc = CMakeToolchain(self)
        # Pass information about OpenVINO from Conan to CMake, if the recipe supports it
        # For example, if the openvino conan recipe sets OPENVINO_DIR in its package_info()
        # tc.variables["OpenVINO_DIR"] = self.dependencies["openvino"].package_folder
        tc.generate()

        # Generates dependency files for find_package (e.g., ZSTDConfig.cmake in generators_folder)
        deps = CMakeDeps(self)
        deps.generate()
