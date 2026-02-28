#!/bin/bash
rm -rf build_test
mkdir build_test && cd build_test
cmake -DTESTING=ON -DSKIP_NFD=ON ..
make test_mod_loader_lzx -j$(nproc)
