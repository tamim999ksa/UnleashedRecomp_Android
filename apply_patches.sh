#!/bin/bash
set -e

# Function to reset and clean a submodule
reset_submodule() {
    local path_var=$1
    local name_var=$2
    if [ -d "$path_var" ]; then
        echo "Resetting $name_var..."
        pushd "$path_var" > /dev/null
        # If it is a submodule, it should have a .git file or directory
        if [ -e ".git" ]; then
             git reset --hard HEAD
             git clean -fdx
        fi
        popd > /dev/null
    fi
}

main() {
    cd "$(dirname "$0")"
    echo "=== Resetting submodules ==="
    reset_submodule "tools/XenonRecomp" "XenonRecomp"
    reset_submodule "tools/XenosRecomp" "XenosRecomp"
    reset_submodule "thirdparty/nativefiledialog-extended" "nativefiledialog-extended"
    reset_submodule "thirdparty/SDL" "SDL"
    reset_submodule "UnleashedRecomp/api" "UnleashedRecomp/api"

    echo "=== Applying overrides ==="
    if [ -d "build_overrides" ]; then
        echo "Copying override files..."
        # We copy overrides AFTER reset to ensure they are present and override submodule files
        cp -rv build_overrides/UnleashedRecomp build_overrides/thirdparty build_overrides/tools .
    fi

    echo "=== Applying patches ==="
    # No patches currently needed as they are covered by build_overrides or direct fixes

    echo "Setting executable permissions..."
    find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-linux" -exec chmod +x {} + 2>/dev/null || true
    find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-macos" -exec chmod +x {} + 2>/dev/null || true
    chmod +x build_android.sh build_tools.sh 2>/dev/null || true
    find tools UnleashedRecomp -name "*.sh" -o -name "*.py" -exec chmod +x {} + 2>/dev/null || true
    echo "=== Patches and overrides applied successfully ==="
}

main "$@"
