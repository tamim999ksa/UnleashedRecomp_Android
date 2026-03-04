#!/bin/bash
set -e

# Make sure we are in the project root
cd "."

echo "=== Resetting submodules ==="

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

# Reset all submodules to ensure a clean state before applying overrides/patches
reset_submodule "tools/XenonRecomp" "XenonRecomp"
reset_submodule "tools/XenosRecomp" "XenosRecomp"
reset_submodule "thirdparty/nativefiledialog-extended" "nativefiledialog-extended"
reset_submodule "thirdparty/SDL" "SDL"
reset_submodule "UnleashedRecomp/api" "UnleashedRecomp/api"

echo "=== Applying overrides ==="
# Copy override files
if [ -d "build_overrides" ]; then
    echo "Copying override files..."
    cp -rv build_overrides/UnleashedRecomp build_overrides/thirdparty build_overrides/tools .
fi

echo "=== Applying patches ==="
# Function to apply a patch safely with a specific directory context and strip level
apply_patch_in() {
    local patch_file="$1"
    local target_dir="$2"
    local strip_level="${3:-1}"
    if [ -f "$patch_file" ]; then
        echo "Applying $(basename "$patch_file") to $target_dir..."
        pushd "$target_dir" > /dev/null
        if ! git apply -p"$strip_level" --check "../../$patch_file" 2>/dev/null; then
             if git apply -p"$strip_level" --reverse --check "../../$patch_file" 2>/dev/null; then
                 echo "Patch already applied, skipping."
             else
                 echo "Warning: Patch failed to apply (check conflicts)."
             fi
        else
            git apply -p"$strip_level" "../../$patch_file"
        fi
        popd > /dev/null
    fi
}

# XenonRecomp patches need -p1 as they are diffed from the submodule root but include top-level dir in path
apply_patch_in "patches/xenon_recomp_fixes.patch" "tools/XenonRecomp" 1
apply_patch_in "patches/xenon_recomp_absolute_branch.patch" "tools/XenonRecomp" 1
apply_patch_in "patches/xenos_recomp_fixes.patch" "tools/XenosRecomp" 1
apply_patch_in "patches/nfd_android.patch" "thirdparty/nativefiledialog-extended" 1
apply_patch_in "patches/sdl_android_fixes.patch" "thirdparty/SDL" 1
apply_patch_in "patches/swa_input_state_fix.patch" "UnleashedRecomp/api" 1
apply_patch_in "patches/hh_string_holder_fix.patch" "UnleashedRecomp/api" 1

# Root project patches
if [ -f "patches/remove_gold_linker_flags.patch" ]; then
    echo "Applying remove_gold_linker_flags.patch..."
    if ! git apply --check "patches/remove_gold_linker_flags.patch" 2>/dev/null; then
         if git apply --reverse --check "patches/remove_gold_linker_flags.patch" 2>/dev/null; then
             echo "Patch already applied, skipping."
         else
             echo "Warning: Patch failed to apply (check conflicts)."
         fi
    else
        git apply "patches/remove_gold_linker_flags.patch"
    fi
fi

# Ensure executable permissions for critical tools
echo "Setting executable permissions for DXC binaries..."
find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-linux" -exec chmod +x {} + 2>/dev/null || true
find tools/XenosRecomp/thirdparty/dxc-bin/bin -type f -name "dxc-macos" -exec chmod +x {} + 2>/dev/null || true

echo "Ensuring scripts are executable..."
chmod +x build_android.sh build_tools.sh 2>/dev/null || true
find tools UnleashedRecomp -name "*.sh" -o -name "*.py" -exec chmod +x {} + 2>/dev/null || true

echo "=== Patches and overrides applied successfully ==="
