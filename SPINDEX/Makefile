# SPINDEX Makefile Wrapper
# Simplifies common development and build tasks

.PHONY: help build rebuild clean test run demo install deps version configure plugins app core backend-install backend-run backend-test all

# Default target
help:
	@echo ""
	@echo "╔══════════════════════════════════════════╗"
	@echo "║        SPINDEX Build System              ║"
	@echo "╚══════════════════════════════════════════╝"
	@echo ""
	@echo "Common targets:"
	@echo "  make build      - Build SPINDEX (incremental)"
	@echo "  make rebuild    - Clean and rebuild everything"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make test       - Run test suite"
	@echo "  make run        - Run SPINDEX with config.ini"
	@echo "  make demo       - Run interactive demo"
	@echo ""
	@echo "Configuration:"
	@echo "  make configure  - Run CMake configuration"
	@echo "  make deps       - Install Conan dependencies"
	@echo ""
	@echo "Utilities:"
	@echo "  make version    - Show version information"
	@echo "  make plugins    - List all plugins"
	@echo "  make install    - Install system dependencies"
	@echo ""

# Configure with CMake
configure:
	@echo "Configuring SPINDEX with CMake..."
	@cmake -S . -B build_cmake \
		-DCMAKE_TOOLCHAIN_FILE=build_cmake/generators/conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DUSE_OPENVINO=OFF \
		-DBUILD_AVX512_PLUGIN=OFF
	@echo "✓ Configuration complete"

# Install Conan dependencies
deps:
	@echo "Installing dependencies with Conan..."
	@conan install . \
		--output-folder=build_cmake \
		--build=missing \
		--settings:host build_type=Release
	@echo "✓ Dependencies installed"

# Build (incremental)
build:
	@echo "Building SPINDEX..."
	@cmake --build build_cmake -j$$(nproc)
	@echo ""
	@echo "✓ Build complete!"
	@echo ""
	@ls -lh build_cmake/bin/spindex_app build_cmake/lib/libspindex_core_lib.a

# Clean build
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build_cmake
	@echo "✓ Clean complete"

# Rebuild from scratch
rebuild: clean deps configure build

# Run tests
test:
	@echo "Running SPINDEX comprehensive test suite..."
	@./run_all_tests.sh

# Run quick plugin test only
test-plugins:
	@./test_plugins.sh

# Build and run feature tests
test-avx512:
	@gcc -o tests/test_avx512_simple tests/test_avx512_simple.c -ldl
	@./tests/test_avx512_simple

test-mmap:
	@gcc -o tests/test_mmap_functionality tests/test_mmap_functionality.c \
		core/mmap_reader.c core/utils.c -Iinclude -Icore -lcurl -ljansson -lm -lz
	@./tests/test_mmap_functionality

# Run the application
run:
	@./build_cmake/bin/spindex_app config.ini

# Run demo
demo:
	@./demo.sh

# Show version
version:
	@./build_cmake/bin/spindex_app --version

# List plugins
plugins:
	@echo "Available plugins:"
	@ls -1 build_cmake/lib/spindex/plugins/*.so | xargs -n1 basename

# Install system dependencies (Debian/Ubuntu)
install:
	@echo "Installing system dependencies..."
	@sudo apt-get update
	@sudo apt-get install -y \
		build-essential \
		cmake \
		python3 \
		python3-pip \
		git \
		libcurl4-openssl-dev \
		redis-server
	@pip3 install conan
	@echo "✓ System dependencies installed"

# Quick rebuild targets
app:
	@cmake --build build_cmake --target spindex_app -j$$(nproc)

core:
	@cmake --build build_cmake --target spindex_core_lib -j$$(nproc)

# Memory-mapped I/O example
mmap-example:
	@echo "Building mmap example..."
	@gcc -o mmap_example core/mmap_example.c core/mmap_reader.c core/utils.c \
		-Iinclude -Icore -pthread -lcurl \
		$$(pkg-config --libs jansson 2>/dev/null || echo "-ljansson") -lm -lz
	@echo "✓ Built mmap_example"
	@echo ""
	@echo "Usage: ./mmap_example <filepath>"
	@echo "Example: ./mmap_example sample_data/readme.txt"

# Backend targets
backend-install:
	@cd ../backend && pip install -r requirements.txt

backend-run:
	@cd ../backend && python main.py

backend-test:
	@cd ../backend && pytest tests/

# Combined targets
all: deps configure build test

.DEFAULT_GOAL := help
