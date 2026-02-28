#!/bin/bash
cmake -B build
cmake --build build --target UnleashedRecomp_Tests
./build/UnleashedRecomp_Tests
