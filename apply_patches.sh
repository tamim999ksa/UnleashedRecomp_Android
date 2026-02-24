#!/bin/bash
set -e
set -x  # Enable debug logging

# Make sure we are in the project root
cd "$(dirname "$0")"

echo "=== Applying overrides ==="

# Function to reset and update submodule
reset_submodule() {
    local SUBMODULE_PATH="$1"
    local SUBMODULE_NAME="$2"

    if [ -e "$SUBMODULE_PATH/.git" ]; then
        echo "Resetting $SUBMODULE_NAME..."
        pushd "$SUBMODULE_PATH" > /dev/null
        git reset --hard HEAD
        git clean -fdx
        # Explicitly sync to handle URL changes
        git submodule sync --recursive
        git submodule update --init --recursive --force
        popd > /dev/null
    fi
}

# Reset submodules to clean state first
reset_submodule "tools/XenonRecomp" "XenonRecomp"

# Explicit check for XenonRecomp critical dependency (fmt)
if [ -e "tools/XenonRecomp/.git" ]; then
    if [ ! -f "tools/XenonRecomp/thirdparty/fmt/CMakeLists.txt" ]; then
        echo "Warning: fmt submodule in XenonRecomp seems missing. Retrying update..."
        pushd "tools/XenonRecomp" > /dev/null
        git submodule update --init --recursive --force
        popd > /dev/null
        if [ ! -f "tools/XenonRecomp/thirdparty/fmt/CMakeLists.txt" ]; then
            echo "Error: Failed to restore fmt submodule!"
            false
        fi
    fi
fi

reset_submodule "tools/XenosRecomp" "XenosRecomp"
reset_submodule "thirdparty/nativefiledialog-extended" "nativefiledialog-extended"
reset_submodule "thirdparty/SDL" "SDL"
reset_submodule "UnleashedRecomp/api" "UnleashedRecomp/api"

# Copy overrides
if [ -d "build_overrides" ]; then
    echo "Copying override files..."
    cp -rv build_overrides/* .
else
    echo "Error: build_overrides directory not found!"
    false
fi

echo "=== Overrides applied successfully ==="
