#!/bin/bash
set -e

# Build script for Android APK

# Make sure we are in the project root
cd "$(dirname "$0")"

echo "Checking dependencies..."
for cmd in 7z python3 wget; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: Required command '$cmd' not found. Please install it."
        false
    fi
done

mkdir -p ./UnleashedRecompLib/private

# Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
if [ -f "./thirdparty/vcpkg/bootstrap-vcpkg.sh" ]; then
    ./thirdparty/vcpkg/bootstrap-vcpkg.sh
fi

# Apply all patches
if [ -f "./apply_patches.sh" ]; then
    chmod +x apply_patches.sh
    ./apply_patches.sh
else
    echo "Error: apply_patches.sh not found!"
    false
fi

# Build host tools if they don't exist
if [ ! -f "./build_tools/bin/XenosRecomp" ]; then
    echo "Building host tools..."
    chmod +x build_tools.sh
    ./build_tools.sh
fi

# Handle private assets
if [ -d "private" ]; then
    echo "Processing local 'private' directory..."
    # Copy all contents to the build directory
    cp -rv private/* ./UnleashedRecompLib/private/ 2>/dev/null || true

    # Recursive extraction (Max 5 levels)
    echo "Checking for nested archives/ISOs/STFS in ./UnleashedRecompLib/private/..."
    for i in {1..15}; do
        nested=$(find ./UnleashedRecompLib/private -type f \( -iname "*.iso" -o -iname "*.7z" -o -iname "*.zip" -o -iname "*.rar" -o -iname "TU_*" -o -iname "*.tar*" -o -iregex ".*/[0-9a-fA-F]\{8,42\}" \) -not -name "*.skipped" -not -name "*.extracted" | head -n 1)

        if [ -z "$nested" ]; then
            break
        fi

        echo "Processing nested item (iteration $i): $nested"

        filename=$(basename "$nested")
        extension="${filename##*.}"
        is_iso=0
        if [[ "$extension" =~ ^(iso|ISO)$ ]]; then
            is_iso=1
        fi

        if 7z x -y "$nested" -o./UnleashedRecompLib/private/ > /dev/null 2>&1; then
            echo "Successfully extracted with 7z: $nested"
            mv "$nested" "${nested}.extracted"
        elif [ "$is_iso" -eq 1 ] && python3 tools/extract_xgd.py "$nested" ./UnleashedRecompLib/private/; then
            echo "Successfully extracted with extract_xgd.py: $nested"
            mv "$nested" "${nested}.extracted"
        elif [ "$is_iso" -eq 0 ] && python3 tools/extract_stfs.py "$nested" ./UnleashedRecompLib/private/; then
            echo "Successfully extracted with extract_stfs.py: $nested"
            mv "$nested" "${nested}.extracted"
        else
            echo "Warning: Failed to extract $nested, skipping..."
            mv "$nested" "${nested}.skipped"
        fi
    done

    # Finalization and normalization
    finalize_file() {
        local filename="$1"
        local target_path="./UnleashedRecompLib/private/$filename"

        # Find all candidates case-insensitively
        local candidates=$(find ./UnleashedRecompLib/private -type f -iname "$filename" | { grep -vFx "$target_path" || true; })
        local candidate_count=$(echo "$candidates" | grep -v "^$" | wc -l || true)

        if [ "$candidate_count" -gt 0 ]; then
            local found_file=""
            if [ "$candidate_count" -gt 1 ]; then
                echo "Warning: Multiple candidates for $filename found:"
                echo "$candidates"
                # Prioritization: Prefer files NOT in 'game' or 'base' folders (likely TUs)
                found_file=$(echo "$candidates" | grep -vE "/(game|base)/" | head -n 1)
                if [ -z "$found_file" ]; then
                    found_file=$(echo "$candidates" | head -n 1)
                fi
                echo "Selected: $found_file"
            else
                found_file="$candidates"
            fi

            if [ -n "$found_file" ]; then
                echo "Moving $found_file to root as $filename..."
                mv -f "$found_file" "$target_path"
            fi
        fi

        # If it's already at the root but with wrong case, fix it
        local root_file=$(find ./UnleashedRecompLib/private -maxdepth 1 -type f -iname "$filename" | head -n 1)
        if [ -n "$root_file" ] && [ "$root_file" != "$target_path" ]; then
             echo "Fixing case for $root_file -> $target_path"
             mv -f "$root_file" "$target_path"
        fi
    }

    finalize_file "default.xex"
    finalize_file "default.xexp"
    finalize_file "shader.ar"
else
    echo "Warning: 'private' directory not found. Using existing files in UnleashedRecompLib/private if present."
fi

# Verification
if [ ! -f "./UnleashedRecompLib/private/default.xex" ] || [ ! -f "./UnleashedRecompLib/private/shader.ar" ]; then
    echo "Error: Mandatory assets (default.xex, shader.ar) are missing!"
    echo "Please place them in a 'private' directory at the root and run again."
    false
fi

# Build APK
echo "Building APK..."
if [ -z "$ANDROID_NDK_HOME" ] && [ -d "$ANDROID_HOME/ndk" ]; then
    NDK_VERSION=$(ls "$ANDROID_HOME/ndk" | tail -n 1)
    if [ -n "$NDK_VERSION" ]; then
        export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/$NDK_VERSION"
        echo "Auto-detected ANDROID_NDK_HOME: $ANDROID_NDK_HOME"
    fi
fi

if [ -f "android/gradlew" ]; then
    chmod +x android/gradlew
    cd android
    ./gradlew assembleDebug
else
    echo "Error: android/gradlew not found!"
    false
fi

echo "Build complete!"
echo "APK located at: android/app/build/outputs/apk/debug/app-debug.apk"
