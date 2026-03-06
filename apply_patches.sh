#!/bin/bash
set -e

# Function to reset and clean a submodule
reset_submodule() {
    local path_var=$1
    local name_var=$2
    if [ -d "$path_var" ]; then
        echo "Resetting $name_var..."
        pushd "$path_var" > /dev/null
        if [ -e ".git" ]; then
             git reset --hard HEAD
             git clean -fdx
        fi
        popd > /dev/null
    fi
}

# Function to apply a patch safely
apply_patch_in() {
    local patch_file=$1
    local target_dir=$2
    local strip_level=${3:-1}

    # Use absolute path for patch file to be safe when using pushd
    local patch_abs_path="$(pwd)/$patch_file"

    if [ -f "$patch_abs_path" ]; then
        echo "Applying $(basename "$patch_file") to $target_dir..."
        pushd "$target_dir" > /dev/null
        if ! git apply --ignore-whitespace --3way -p"$strip_level" "$patch_abs_path"; then
             if git apply --ignore-whitespace --3way -p"$strip_level" --reverse --check "$patch_abs_path" > /dev/null 2>&1; then
                 echo "Patch already applied, skipping."
                 popd > /dev/null
                 return 0
             else
                 echo "ERROR: Patch failed to apply to $target_dir."
                 popd > /dev/null
                 return 1
             fi
        fi
        popd > /dev/null
    else
        echo "WARNING: Patch file $patch_abs_path not found."
    fi
    return 0
}

main() {
    cd "$(dirname "$0")"
    echo "=== Resetting submodules ==="
    reset_submodule "tools/XenonRecomp" "XenonRecomp"
    reset_submodule "tools/XenosRecomp" "XenosRecomp"
    reset_submodule "thirdparty/nativefiledialog-extended" "nativefiledialog-extended"
    reset_submodule "thirdparty/SDL" "SDL"
    reset_submodule "UnleashedRecomp/api" "UnleashedRecomp/api"

    echo "=== Applying patches ==="
    apply_patch_in "patches/xenon_recomp_fixes.patch" "tools/XenonRecomp" 1 || return 1
    apply_patch_in "patches/xenon_recomp_absolute_branch.patch" "tools/XenonRecomp" 1 || return 1

    echo "=== Applying overrides ==="
    if [ -d "build_overrides" ]; then
        echo "Copying override files..."
        cp -rv build_overrides/UnleashedRecomp build_overrides/thirdparty build_overrides/tools .
    fi

    echo "Setting executable permissions..."
    find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-linux" -exec chmod +x {} + 2>/dev/null || true
    find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-macos" -exec chmod +x {} + 2>/dev/null || true
    chmod +x build_android.sh build_tools.sh 2>/dev/null || true
    find tools UnleashedRecomp -name "*.sh" -o -name "*.py" -exec chmod +x {} + 2>/dev/null || true
    echo "=== Patches and overrides applied successfully ==="
}

main "$@"
