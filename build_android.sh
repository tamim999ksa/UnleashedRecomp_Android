#!/bin/bash
set -e

# Build script for Android APK

# Check for private assets
if [ -d "private" ]; then
    echo "Copying private assets..."
    mkdir -p UnleashedRecompLib/private

    echo "Searching for default.xex..."
    find private -iname "default.xex" -exec cp {} UnleashedRecompLib/private/default.xex \;

    echo "Searching for default.xexp..."
    find private -iname "default.xexp" -exec cp {} UnleashedRecompLib/private/default.xexp \;

    echo "Searching for shader.ar..."
    find private -iname "shader.ar" -exec cp {} UnleashedRecompLib/private/shader.ar \;
else
    echo "Warning: 'private' directory not found. Build may fail or be incomplete."
fi

# Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
./thirdparty/vcpkg/bootstrap-vcpkg.sh

# Apply all patches
if [ -f "./apply_patches.sh" ]; then
    ./apply_patches.sh
else
    echo "Error: apply_patches.sh not found!"
    false
fi

# Build APK
echo "Building APK..."
cd android
./gradlew assembleDebug

echo "Build complete!"
echo "APK located at: android/app/build/outputs/apk/debug/app-debug.apk"
