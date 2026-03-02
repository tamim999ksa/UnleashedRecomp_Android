#!/bin/bash
set -e

# Setup include paths
ROOT_DIR="$(dirname "$(readlink -f "$0")")/../.."

# Shift the first argument which is the main test file
MAIN_TEST_FILE="$1"
shift

# Compile the test with any additional source files passed
g++ -std=c++20 \
    -DFMT_HEADER_ONLY \
    -DZSTD_DISABLE_ASM \
    -I$ROOT_DIR \
    -I$ROOT_DIR/UnleashedRecomp \
    -I$ROOT_DIR/UnleashedRecomp/kernel \
    -I$ROOT_DIR/UnleashedRecomp/cpu \
    -I$ROOT_DIR/tools/XenonRecomp/XenonUtils \
    -I$ROOT_DIR/tools/XenonRecomp/thirdparty/simde \
    -I$ROOT_DIR/tools/XenonRecomp/thirdparty/fmt/include \
    -I$ROOT_DIR/tools/XenosRecomp/thirdparty/zstd/lib \
    -I$ROOT_DIR/tools/XenonRecomp/thirdparty/xxHash \
    -I$ROOT_DIR/thirdparty/ankerl/unordered_dense/include \
    -I$ROOT_DIR/thirdparty/boost/include \
    -I$ROOT_DIR/thirdparty/magic_enum/include \
    -I$ROOT_DIR/thirdparty/SDL/include \
    -D_GNU_SOURCE \
    -Wno-psabi \
    "$MAIN_TEST_FILE" "$@" -o test_binary

echo "Compilation successful."

# Run the test
./test_binary

# Cleanup
rm test_binary
