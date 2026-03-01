#!/bin/bash
set -e

# Setup include paths
ROOT_DIR="$(dirname "$(readlink -f "$0")")/../.."

# Shift the first argument which is the main test file
MAIN_TEST_FILE="$1"
shift

# Compile the test with any additional source files passed
g++ -std=c++20 \
    -I$ROOT_DIR \
    -I$ROOT_DIR/UnleashedRecomp \
    -I$ROOT_DIR/UnleashedRecomp/kernel \
    -I$ROOT_DIR/UnleashedRecomp/cpu \
    -I$ROOT_DIR/tools/XenonRecomp/XenonUtils \
    -I$ROOT_DIR/tools/XenonRecomp/thirdparty/simde \
    -D_GNU_SOURCE \
    -Wno-psabi \
    "$MAIN_TEST_FILE" "$@" -o test_binary

echo "Compilation successful."

# Run the test
./test_binary

# Cleanup
rm test_binary
