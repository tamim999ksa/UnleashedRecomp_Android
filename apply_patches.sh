#!/bin/bash
set -e

# Make sure we are in the project root
cd "$(dirname "$0")"

echo "=== Applying overrides ==="

# Function to reset and clean a submodule
reset_submodule() {
    local SUBMODULE_PATH=$1
    local SUBMODULE_NAME=$2
    if [ -e "$SUBMODULE_PATH/.git" ]; then
        echo "Resetting $SUBMODULE_NAME..."
        pushd "$SUBMODULE_PATH" > /dev/null
        git reset --hard HEAD
        git clean -fdx
        git submodule sync --recursive
        git submodule update --init --recursive --force
        popd > /dev/null
    fi
}

# Reset all submodules to ensure a clean state before applying overrides
reset_submodule "tools/XenonRecomp" "XenonRecomp"
reset_submodule "tools/XenosRecomp" "XenosRecomp"
reset_submodule "thirdparty/nativefiledialog-extended" "nativefiledialog-extended"
reset_submodule "thirdparty/SDL" "SDL"
reset_submodule "UnleashedRecomp/api" "UnleashedRecomp/api"

# Copy override files
if [ -d "build_overrides" ]; then
    echo "Copying override files..."
    cp -rv build_overrides/UnleashedRecomp build_overrides/thirdparty build_overrides/tools .
fi

# Ensure executable permissions for critical tools
echo "Setting executable permissions for DXC binaries..."
find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-linux" -exec chmod +x {} +
find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-macos" -exec chmod +x {} +

echo "Ensuring scripts are executable..."
chmod +x build_android.sh build_tools.sh build_overrides/apply_patches.sh 2>/dev/null || true
find tools -name "*.sh" -o -name "*.py" -exec chmod +x {} +

echo "=== Overrides applied successfully ==="
