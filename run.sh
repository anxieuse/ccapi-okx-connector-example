#!/bin/bash

echo "========================================"
echo "| CCAPI OKX EXAMPLE - RUN SCRIPT        |"
echo "========================================"
echo

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
# Check if build directory exists
if [ ! -d "build" ]; then
    echo "ERROR: Build directory not found!"
    echo "Please run ./compile.sh first to build the project."
    exit 1
fi

# Check if executable exists
if [ ! -f "build/ccapi-okx-example" ]; then
    echo "ERROR: Executable not found!"
    echo "Please run ./compile.sh to build the project."
    exit 1
fi

# Check if executable is actually executable
if [ ! -x "build/ccapi-okx-example" ]; then
    echo "Making executable file executable..."
    chmod +x build/ccapi-okx-example
fi

# Set OKX API credentials
echo "Setting OKX API credentials..."
# # prod, money
# export OKX_API_KEY="a"
# export OKX_API_SECRET="b"
# export OKX_API_PASSPHRASE="c"

# # prod, no money
# export OKX_API_KEY="a"
# export OKX_API_SECRET="b"
# export OKX_API_PASSPHRASE="c"

# demo
export OKX_API_KEY="a"
export OKX_API_SECRET="b"
export OKX_API_PASSPHRASE="c"
export OKX_API_X_SIMULATED_TRADING="1"

# Parse command line arguments
SLEEP_TIME=${1:-15}  # Default to 15 seconds if no argument provided

# Run the application
echo "Starting CCAPI OKX performance test..."
echo "This will run for $SLEEP_TIME seconds and test order placement latency."
echo "Usage: ./run.sh [sleep_time_in_seconds]"
echo
echo "========================================"
echo

# Execute the program with sleep time argument
./build/ccapi-okx-example $SLEEP_TIME

# Check exit status
if [ $? -eq 0 ]; then
    echo
    echo "========================================"
    echo "| PROGRAM COMPLETED SUCCESSFULLY!      |"
    echo "========================================"
else
    echo
    echo "ERROR: Program failed with exit code $?"
    echo "Check the error messages above for details."
    exit 1
fi 