#!/bin/bash

# OurTaiko Debug Build Script
# Builds the project in Debug mode with sanitizers and copies the executable to root

set -e  # Exit on error

echo "Building OurTaiko (Debug)..."
echo ""

# Clean and build
rm -rf build
find .cmake-deps -maxdepth 1 -name '*-subbuild' -type d -exec rm -rf {} + 2>/dev/null || true
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja \
  $(command -v ccache &>/dev/null && echo "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
cmake --build build -j"${JOBS:-$(( $(nproc) / 2 ))}"

# Copy executable to root directory
if [ -f build/bin/OurTaiko ]; then
    echo ""
    echo "Copying executable to root directory..."
    cp build/bin/OurTaiko ./OurTaiko
    cp build/bin/LICENSE build/bin/NOTICE .
    chmod +x ./OurTaiko
    if [ -d build/bin/OurTaiko.app ]; then
        cp -R build/bin/OurTaiko.app .
    fi
    if [ -f build/bin/OurTaiko.png ]; then
        cp build/bin/OurTaiko.png build/bin/install-desktop-entry.py .
    fi
    echo "Build complete! Executable is ready at ./OurTaiko"
else
    echo "Error: Executable not found at build/bin/OurTaiko"
    exit 1
fi
