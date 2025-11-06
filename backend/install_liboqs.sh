#!/bin/bash
#
# SPINDEX liboqs Installation Script
# Installs the Open Quantum Safe liboqs library for ML-KEM-1024 and ML-DSA-87 support
#

set -e

echo "================================"
echo "Installing liboqs for SPINDEX PQC"
echo "================================"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root or with sudo"
    exit 1
fi

# Install build dependencies
echo "Installing build dependencies..."
apt-get update
apt-get install -y \
    cmake \
    gcc \
    g++ \
    ninja-build \
    libssl-dev \
    python3-pytest \
    python3-pytest-xdist \
    unzip \
    xsltproc \
    doxygen \
    graphviz \
    git

# Create temporary build directory
BUILD_DIR=$(mktemp -d)
cd "$BUILD_DIR"

echo "Cloning liboqs from GitHub..."
git clone -b main https://github.com/open-quantum-safe/liboqs.git
cd liboqs

echo "Building liboqs..."
mkdir build
cd build

# Configure with ML-KEM and ML-DSA enabled
cmake -GNinja \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DOQS_USE_OPENSSL=ON \
    -DOQS_BUILD_ONLY_LIB=ON \
    ..

echo "Compiling (this may take a few minutes)..."
ninja

echo "Installing to /usr/local..."
ninja install

# Update library cache
echo "Updating library cache..."
ldconfig

# Verify installation
echo "Verifying installation..."
if ldconfig -p | grep -q liboqs; then
    echo "✓ liboqs successfully installed"
else
    echo "✗ liboqs installation verification failed"
    exit 1
fi

# Cleanup
cd /
rm -rf "$BUILD_DIR"

echo ""
echo "================================"
echo "Installation Complete!"
echo "================================"
echo ""
echo "Next steps:"
echo "1. Verify Python bindings: python3 -c 'import oqs; print(oqs.get_enabled_KEM_mechanisms())'"
echo "2. Test PQC module: python3 backend/pqcrypto.py"
echo "3. Generate keys: python3 backend/pqcrypto_example.py"
echo ""
