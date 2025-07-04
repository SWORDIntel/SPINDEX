# --- Builder Stage ---
FROM python:3.9-bullseye-slim AS builder

LABEL maintainer="JulesAI"
LABEL description="Builder stage for SPINDEX C Worker"

ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONUNBUFFERED=1 \
    CONAN_USER_HOME=/opt/conan \
    CONAN_STORAGE_PATH=/opt/conan/data

# Install build essentials, cmake, git, and ca-certificates
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Conan (Conan 2.x)
RUN pip install --no-cache-dir "conan>=2.0,<2.2" # Allow minor updates in 2.x series

WORKDIR /app

# Copy conanfile.py first for caching
COPY conanfile.py ./

# Install Conan dependencies
RUN conan install . --output-folder=build --build=missing -s:h build_type=Release -s:b build_type=Release \
    && conan profile detect --force # Ensure a default profile exists for the build

# Copy the rest of the SPINDEX C/C++ source code
# A more refined copy would list specific directories like core/, include/, plugins/
# and the root CMakeLists.txt to better leverage Docker caching.
COPY CMakeLists.txt ./
COPY core ./core
COPY include ./include
COPY plugins ./plugins
# COPY tests ./tests # Not needed if BUILD_TESTS=OFF

# Configure and build SPINDEX C worker and plugins
# Using -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON for more build output if needed for debugging
RUN cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF && \
    cmake --build build --target spindex_app spindex_plugins_list -j $(nproc)

# --- Runtime Stage ---
FROM debian:bullseye-slim AS runtime

LABEL maintainer="JulesAI"
LABEL description="Runtime stage for SPINDEX C Worker"

ENV DEBIAN_FRONTEND=noninteractive \
    SPINDEX_PLUGIN_DIR=/opt/spindex/plugins \
    SPINDEX_CONFIG_FILE=/opt/spindex/config/spindex.conf \
    LD_LIBRARY_PATH=/opt/spindex/lib:${LD_LIBRARY_PATH} \
    LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libstdc++6 \
    # Add any other absolutely essential runtime system libraries if identified
    && rm -rf /var/lib/apt/lists/*

RUN groupadd --system spindex && \
    useradd --system --gid spindex --home-dir /opt/spindex --shell /sbin/nologin spindex

WORKDIR /opt/spindex

# Create necessary directories
RUN mkdir -p bin plugins lib config logs data && \
    chown -R spindex:spindex /opt/spindex

# Copy compiled application and plugins
COPY --from=builder --chown=spindex:spindex /app/build/bin/spindex_app /opt/spindex/bin/
COPY --from=builder --chown=spindex:spindex /app/build/lib/spindex/plugins/*.so /opt/spindex/plugins/

# --- Copy runtime dependencies from Conan packages ---
# This section is critical and highly dependent on the actual Conan package structures.
# Using a temporary deployment directory in the builder stage is more robust.

# Example: Create a deployment directory in builder and use conan deploy
# In Builder Stage (before # --- Runtime Stage ---):
# RUN mkdir /app/deploy_runtime && \
#     conan deploy . --output-folder=/app/deploy_runtime --deployable=spindex_app --deployable=spindex_plugins_list -g DeployerTxt -of /app/deploy_runtime
# This would require a custom deployer or careful selection.
# For now, direct copy with wildcards, acknowledging its fragility.

# Jansson
COPY --from=builder --chown=spindex:spindex /opt/conan/data/jansson/*/package/*/lib/libjansson.so* /opt/spindex/lib/
# libcurl (and its dependencies like zlib, openssl etc. if not system-linked by curl recipe)
COPY --from=builder --chown=spindex:spindex /opt/conan/data/libcurl/*/package/*/lib/libcurl.so* /opt/spindex/lib/
COPY --from=builder --chown=spindex:spindex /opt/conan/data/zlib/*/package/*/lib/libz.so* /opt/spindex/lib/ # Example dep of curl
# GEOS
COPY --from=builder --chown=spindex:spindex /opt/conan/data/geos/*/package/*/lib/libgeos_c.so* /opt/spindex/lib/
COPY --from=builder --chown=spindex:spindex /opt/conan/data/geos/*/package/*/lib/libgeos.so* /opt/spindex/lib/
# Libzip
COPY --from=builder --chown=spindex:spindex /opt/conan/data/libzip/*/package/*/lib/libzip.so* /opt/spindex/lib/
# Zstd
COPY --from=builder --chown=spindex:spindex /opt/conan/data/zstd/*/package/*/lib/libzstd.so* /opt/spindex/lib/

# OpenVINO: This is highly complex. A proper conan deploy or very detailed copy is needed.
# The following are placeholders and likely insufficient.
COPY --from=builder --chown=spindex:spindex /opt/conan/data/openvino/*/package/*/lib/libopenvino*.so* /opt/spindex/lib/
# OpenVINO might also need other files from its package (e.g., plugins.xml, TBB libraries, etc.)
# COPY --from=builder /opt/conan/data/openvino/*/package/*/runtime /opt/spindex/openvino_runtime # Example

# Ensure all copied libraries have correct permissions (executable bit might be lost)
RUN chmod +x /opt/spindex/bin/spindex_app && \
    find /opt/spindex/lib /opt/spindex/plugins -name '*.so*' -exec chmod +r-x {} \; -exec chmod +r {} \;


# Copy a default configuration file
# Assume it's located at the root of SPINDEX project for now, named 'spindex_docker.conf'
COPY --chown=spindex:spindex test_spindex_es.conf ${SPINDEX_CONFIG_FILE}
# If the config file isn't found, the app might fail. Provide a default or ensure it's mounted.

USER spindex

ENTRYPOINT ["/opt/spindex/bin/spindex_app"]
CMD ["--config-file", "/opt/spindex/config/spindex.conf"]
