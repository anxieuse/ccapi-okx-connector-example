#!/bin/bash

echo "========================================"
echo "| CCAPI OKX EXAMPLE - BUILD SCRIPT     |"
echo "========================================"
echo

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed or not in PATH"
    echo "Please install CMake 3.14 or later"
    exit 1
fi

# Check CMake version
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
echo "Found CMake version: $CMAKE_VERSION"

# Get the script directory to ensure we're in the right place
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Working directory: $(pwd)"

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Clean previous build if it exists
echo "Cleaning previous build artifacts..."
# Preserve downloaded dependencies (boost, rapidjson, hffix) but clean build artifacts
find . -maxdepth 1 -type f -delete  # Remove all files in build directory
find . -maxdepth 1 -type d ! -name "." ! -name "boost" ! -name "rapidjson" ! -name "hffix" -exec rm -rf {} + 2>/dev/null || true

# Configure the project
echo
echo "Configuring project with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo
    echo "ERROR: CMake configuration failed!"
    echo "Common solutions:"
    echo "1. Install required dependencies:"
    echo "   - Ubuntu/Debian: sudo apt-get install build-essential cmake libssl-dev zlib1g-dev"
    echo "   - CentOS/RHEL: sudo yum groupinstall \"Development Tools\" && sudo yum install cmake3 openssl-devel zlib-devel"
    echo "   - macOS: brew install cmake openssl"
    echo "2. Set OPENSSL_ROOT_DIR if needed: cmake .. -DOPENSSL_ROOT_DIR=/path/to/openssl"
    exit 1
fi

# Build the project
echo
echo "Building project..."
make -j$(nproc 2>/dev/null || echo 2)

if [ $? -eq 0 ]; then
    echo
    echo "========================================"
    echo "| BUILD COMPLETED SUCCESSFULLY!        |"
    echo "========================================"
    echo "Executable: build/ccapi-okx-example"
    echo "Run with: ./run.sh"
    echo
else
    echo
    echo "ERROR: Build failed!"
    echo "Check the error messages above for details."
    exit 1
fi 