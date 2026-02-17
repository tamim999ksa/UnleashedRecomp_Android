#!/bin/bash
set -e

# Setup include paths
ROOT_DIR="$(dirname "$(readlink -f "$0")")/../.."

# Compile the test
g++ -std=c++20 \
    -I$ROOT_DIR \
    -I$ROOT_DIR/UnleashedRecomp \
    -I$ROOT_DIR/UnleashedRecomp/kernel \
    -I$ROOT_DIR/UnleashedRecomp/cpu \
    -I$ROOT_DIR/tools/XenonRecomp/XenonUtils \
    -I$ROOT_DIR/tools/XenonRecomp/thirdparty/simde \
    -D_GNU_SOURCE \
    -Wno-psabi \
    guest_stack_var_test.cpp -o guest_stack_var_test

echo "Compilation successful."

# Run the test
./guest_stack_var_test

# Cleanup
rm guest_stack_var_test
