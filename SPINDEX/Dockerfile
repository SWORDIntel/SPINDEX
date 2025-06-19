# Stage 1: Builder
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    cmake \
    gcc \
    g++ \
    make \
    pkg-config \
    curl \
    python3 \
    python3-pip \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Conan (v2)
RUN pip3 install conan==2.0.17

# Copy the entire project context. Assumes Dockerfile is in SPINDEX/
# So, ../ copies the root of the repository.
COPY ../ /app/

WORKDIR /app/SPINDEX

# Conan install. This assumes conanfile.py is in /app/SPINDEX/
# The output folder 'cmake_build' will contain the toolchain file and other generators.
RUN conan install . --output-folder=cmake_build --build=missing --settings:host build_type=Release

# Configure and build SPINDEX using CMake
# The toolchain file is expected at cmake_build/generators/conan_toolchain.cmake
RUN cmake -S . -B cmake_build \
    -DCMAKE_TOOLCHAIN_FILE=cmake_build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/app/install_dir
RUN cmake --build cmake_build --config Release --target install

# Stage 2: Runtime
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
# Minimal set; ldd on spindex_app and plugins in builder stage can verify full list.
# libjansson4 is for Jansson, libcurl4 for Curl.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libjansson4 \
    ca-certificates \
    # Add any other system runtime libraries identified by ldd if not provided by Conan build
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user and group
RUN groupadd -r spindex_user && useradd --no-log-init -r -g spindex_user spindex_user

# Create application directories
RUN mkdir -p /opt/spindex/bin \
             /opt/spindex/lib \
             /opt/spindex/config \
             /opt/spindex/data/test_data \
             /var/log/spindex

# Copy built application, plugins, and necessary shared libs from builder stage
# Assumes 'cmake --target install' placed them correctly under CMAKE_INSTALL_PREFIX
COPY --from=builder /app/install_dir/bin/spindex_app /opt/spindex/bin/
COPY --from=builder /app/install_dir/lib/ /opt/spindex/lib/

# Copy configuration file (created in a previous step) and test_data
# These paths are relative to the Dockerfile's context (SPINDEX directory)
COPY test_spindex_docker.conf /opt/spindex/config/spindex.conf
COPY test_data /opt/spindex/data/test_data/

# Set ownership for application and log directories
RUN chown -R spindex_user:spindex_user /opt/spindex /var/log/spindex

# Switch to non-root user
USER spindex_user

# Set working directory
WORKDIR /opt/spindex

# Define default command (config file path)
# spindex_app expects the config file as an argument.
ENTRYPOINT ["./bin/spindex_app"]
CMD ["config/spindex.conf"]
