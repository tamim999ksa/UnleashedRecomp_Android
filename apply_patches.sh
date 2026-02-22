#!/bin/bash
set -e

# Make sure we are in the project root
cd "$(dirname "$0")"

echo "=== Applying patches ==="

# Apply NFD Android Patch
echo "Applying nativefiledialog-extended Android patch..."
cd thirdparty/nativefiledialog-extended
if ! grep -q "PLATFORM_ANDROID" CMakeLists.txt; then
    git apply ../../patches/nfd_android.patch
else
    echo "Patch already applied."
fi
cd ../..

# Apply XenonRecomp Patch
echo "Applying XenonRecomp fixes..."
cd tools/XenonRecomp
echo "Resetting XenonRecomp..."
git reset --hard HEAD && git clean -fdx
if ! grep -q "VERSION 3.10" CMakeLists.txt; then
    echo "Applying patch via git apply..."
    if ! git apply --ignore-whitespace ../../patches/xenon_recomp_fixes.patch; then
        echo "git apply failed, attempting to use patch..."
        patch -p1 < ../../patches/xenon_recomp_fixes.patch
    fi
else
    echo "XenonRecomp fixes already applied (should not happen after reset)."
fi
cd ../..

# Apply XenosRecomp Patch
echo "Applying XenosRecomp fixes..."
cd tools/XenosRecomp
echo "Resetting XenosRecomp..."
git reset --hard HEAD && git clean -fdx

# Debug: Check content before
echo "Checking XenosRecomp/shader_recompiler.cpp before patch..."
if grep -q "inst.vertexFetch" XenosRecomp/shader_recompiler.cpp; then
    echo "WARNING: File already seems patched before application!"
else
    echo "File is clean (unpatched)."
fi

# Apply patch
echo "Applying patch via git apply..."
if ! git apply --ignore-whitespace ../../patches/xenos_recomp_fixes.patch; then
    echo "git apply failed, attempting to use patch..."
    patch -p1 < ../../patches/xenos_recomp_fixes.patch
fi

# Debug: Check content after
echo "Checking XenosRecomp/shader_recompiler.cpp after patch..."
# This command will fail the script if grep doesn't find the string
grep -q "inst.vertexFetch" XenosRecomp/shader_recompiler.cpp
echo "SUCCESS: File patched successfully."

cd ../..

# Apply SDL Android Patch
echo "Applying SDL Android fixes..."
cd thirdparty/SDL
if ! grep -q "ASensorManager_getInstanceForPackage" src/sensor/android/SDL_androidsensor.c; then
    echo "Applying patch via git apply..."
    if ! git apply ../../patches/sdl_android_fixes.patch; then
        echo "git apply failed, attempting to use patch..."
        patch -p1 < ../../patches/sdl_android_fixes.patch
    fi
else
    echo "SDL Android fixes already applied."
fi
cd ../..

# Apply Hedgehog String Holder Patch
echo "Applying Hedgehog String Holder patch..."
cd UnleashedRecomp/api
if ! grep -q "std::memcpy" Hedgehog/Base/Type/detail/hhStringHolder.inl; then
    echo "Applying patch via git apply..."
    if ! git apply ../../patches/hh_string_holder_fix.patch; then
        echo "git apply failed, attempting to use patch..."
        patch -p1 < ../../patches/hh_string_holder_fix.patch
    fi
else
    echo "Hedgehog String Holder patch already applied."
fi
cd ../..

echo "=== Patches applied successfully ==="
