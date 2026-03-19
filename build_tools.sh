#!/bin/bash
set -e

# Make sure we are in the project root
cd "$(dirname "$0")"

# Define absolute path for output binaries
OUTPUT_BIN_DIR="$(pwd)/build_tools/bin"
mkdir -p "$OUTPUT_BIN_DIR"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" == "x86_64" ] || [ "$ARCH" == "amd64" ] || [ "$ARCH" == "x64" ]; then
    DXC_ARCH="x64"
elif [ "$ARCH" == "aarch64" ] || [ "$ARCH" == "arm64" ]; then
    DXC_ARCH="arm64"
else
    echo "Unsupported architecture: $ARCH"
    false
fi

# Apply all patches
if [ -f "./apply_patches.sh" ]; then
    chmod +x apply_patches.sh
    ./apply_patches.sh
else
    echo "Error: apply_patches.sh not found!"
    false
fi

# Install function using absolute path
copy_tool() {
    local tool_name=$1
    local tool_path=$(find . -name "$tool_name" -type f -executable | head -n 1)
    if [ -n "$tool_path" ]; then
        cp "$tool_path" "$OUTPUT_BIN_DIR/"
        echo "Copied $tool_name to $OUTPUT_BIN_DIR"
    else
        echo "Error: Could not find executable for $tool_name"
        false
    fi
}

echo "=== Building GCC compatible tools ==="
rm -rf build_tools/build_gcc
mkdir -p build_tools/build_gcc
cd build_tools/build_gcc

export CC=gcc
export CXX=g++

# Configure
cmake ../.. -DCMAKE_BUILD_TYPE=Release -DBUILD_TOOLS_ONLY=ON

# Build
cmake --build . --target file_to_c fshasher x_decompress bc_diff --parallel 2

copy_tool "file_to_c"
copy_tool "fshasher"
copy_tool "x_decompress"
copy_tool "bc_diff"

cd ../..

echo "=== Building host tools (XenonRecomp/XenosRecomp) ==="
rm -rf build_tools/build_clang
mkdir -p build_tools/build_clang
cd build_tools/build_clang

export CC=gcc
export CXX=g++

# Configure
cmake ../.. -DCMAKE_BUILD_TYPE=Release -DBUILD_TOOLS_ONLY=ON

# Build
cmake --build . --target XenonRecomp XenosRecomp --parallel 2

# Install
copy_tool "XenonRecomp"
copy_tool "XenosRecomp"

cd ../..

# Copy libdxcompiler.so and libdxil.so from the correct architecture directory
cp -v "tools/XenosRecomp/thirdparty/dxc-bin/lib/$DXC_ARCH/libdxcompiler.so" "$OUTPUT_BIN_DIR/"
echo "Copied libdxcompiler.so ($DXC_ARCH)"

if [ -f "tools/XenosRecomp/thirdparty/dxc-bin/lib/$DXC_ARCH/libdxil.so" ]; then
    cp -v "tools/XenosRecomp/thirdparty/dxc-bin/lib/$DXC_ARCH/libdxil.so" "$OUTPUT_BIN_DIR/"
    echo "Copied libdxil.so ($DXC_ARCH)"
fi

echo "Build tools ready in $OUTPUT_BIN_DIR"
ls -l "$OUTPUT_BIN_DIR"

# Patch RPATH for XenosRecomp
if command -v patchelf >/dev/null 2>&1; then
    patchelf --set-rpath '$ORIGIN' "$OUTPUT_BIN_DIR/XenosRecomp"
    echo "Patched RPATH for XenosRecomp"
else
    echo "Warning: patchelf not found, skipping RPATH patch"
fi
