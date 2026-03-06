#!/bin/bash
set -e

# Function to reset and clean a submodule
reset_submodule() {
    local path_var=$1
    local name_var=$2
    if [ -e "$path_var/.git" ]; then
        echo "Resetting $name_var..."
        pushd "$path_var" > /dev/null
        git reset --hard HEAD
        git clean -fdx
        git submodule sync --recursive
        git submodule update --init --recursive --force
        popd > /dev/null
    fi
}

# Function to apply a patch safely with a specific directory context and strip level
apply_patch_in() {
    local patch_file="$1"
    local target_dir="$2"
    local strip_level="${3:-1}"
    if [ -f "$patch_file" ]; then
        echo "Applying $(basename "$patch_file") to $target_dir..."
        pushd "$target_dir" > /dev/null
        # Use --ignore-whitespace and --3way to be more robust
        if ! git apply --ignore-whitespace --3way -p"$strip_level" --check "../../$patch_file" 2>&1; then
             # Check if already applied
             if git apply --ignore-whitespace --3way -p"$strip_level" --reverse --check "../../$patch_file" > /dev/null 2>&1; then
                 echo "Patch already applied, skipping."
                 popd > /dev/null
                 return 0
             else
                 echo "ERROR: Patch failed to apply to $target_dir (check conflicts)."
                 popd > /dev/null
                 return 1
             fi
        else
            if ! git apply --ignore-whitespace --3way -p"$strip_level" "../../$patch_file"; then
                echo "ERROR: Failed to apply patch after check passed."
                popd > /dev/null
                return 1
            fi
        fi
        popd > /dev/null
    fi
    return 0
}

main() {
    # Make sure we are in the project root
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
        cp -rv build_overrides/UnleashedRecomp build_overrides/thirdparty build_overrides/tools .
    fi

    echo "=== Applying patches ==="
    # These patches are critical, fail early if they fail to apply
    apply_patch_in "patches/xenon_recomp_fixes.patch" "tools/XenonRecomp" 1 || return 1
    apply_patch_in "patches/xenon_recomp_absolute_branch.patch" "tools/XenonRecomp" 1 || return 1
    apply_patch_in "patches/xenos_recomp_fixes.patch" "tools/XenosRecomp" 1 || return 1
    apply_patch_in "patches/nfd_android.patch" "thirdparty/nativefiledialog-extended" 1 || return 1
    apply_patch_in "patches/sdl_android_fixes.patch" "thirdparty/SDL" 1 || return 1
    apply_patch_in "patches/swa_input_state_fix.patch" "UnleashedRecomp/api" 1 || return 1
    apply_patch_in "patches/hh_string_holder_fix.patch" "UnleashedRecomp/api" 1 || return 1

    # Root project patches
    if [ -f "patches/remove_gold_linker_flags.patch" ]; then
        echo "Applying remove_gold_linker_flags.patch..."
        if ! git apply --ignore-whitespace --3way --check "patches/remove_gold_linker_flags.patch" > /dev/null 2>&1; then
             if git apply --ignore-whitespace --3way --reverse --check "patches/remove_gold_linker_flags.patch" > /dev/null 2>&1; then
                 echo "Patch already applied, skipping."
             else
                 echo "ERROR: Patch failed to apply to root (check conflicts)."
                 return 1
             fi
        else
            git apply --ignore-whitespace --3way "patches/remove_gold_linker_flags.patch"
        fi
    fi

    echo "Setting executable permissions..."
    find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-linux" -exec chmod +x {} + 2>/dev/null || true
    find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-macos" -exec chmod +x {} + 2>/dev/null || true
    chmod +x build_android.sh build_tools.sh 2>/dev/null || true
    find tools UnleashedRecomp -name "*.sh" -o -name "*.py" -exec chmod +x {} + 2>/dev/null || true

    echo "=== Patches and overrides applied successfully ==="
}

main "$@"
