#!/bin/bash
set -e

# Make sure we are in the project root
cd "$(dirname "$0")"

echo "=== Applying overrides ==="

# Reset submodules to clean state first
# XenonRecomp
if [ -e "tools/XenonRecomp/.git" ]; then
    echo "Resetting XenonRecomp..."
    cd tools/XenonRecomp && git reset --hard HEAD && git clean -fdx && git submodule update --init --recursive && cd ../..
fi

# XenosRecomp
if [ -e "tools/XenosRecomp/.git" ]; then
    echo "Resetting XenosRecomp..."
    cd tools/XenosRecomp && git reset --hard HEAD && git clean -fdx && git submodule update --init --recursive && cd ../..
fi

# nativefiledialog-extended
if [ -e "thirdparty/nativefiledialog-extended/.git" ]; then
    echo "Resetting nativefiledialog-extended..."
    cd thirdparty/nativefiledialog-extended && git reset --hard HEAD && git clean -fdx && git submodule update --init --recursive && cd ../..
fi

# SDL
if [ -e "thirdparty/SDL/.git" ]; then
    echo "Resetting SDL..."
    cd thirdparty/SDL && git reset --hard HEAD && git clean -fdx && git submodule update --init --recursive && cd ../..
fi

# UnleashedRecomp/api
if [ -e "UnleashedRecomp/api/.git" ]; then
    echo "Resetting UnleashedRecomp/api..."
    cd UnleashedRecomp/api && git reset --hard HEAD && git clean -fdx && git submodule update --init --recursive && cd ../..
fi

# Copy overrides
if [ -d "build_overrides" ]; then
    echo "Copying override files..."
    cp -rv build_overrides/* .
else
    echo "Error: build_overrides directory not found!"
    false
fi

echo "=== Overrides applied successfully ==="
