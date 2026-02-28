#!/bin/bash
g++ -fsyntax-only -std=c++20 \
  -I UnleashedRecomp/tests/mock \
  -I UnleashedRecomp \
  -I thirdparty \
  -I tools/XenonRecomp/XenonUtils \
  -I UnleashedRecomp/api \
  -I thirdparty/unordered_dense/include \
  -I tools/XenonRecomp/thirdparty/fmt/include \
  -I tools/XenonRecomp/thirdparty/tomlplusplus/include \
  -I tools/XenonRecomp/thirdparty/xxHash \
  -I thirdparty/SDL/include \
  -DXXH_INLINE_ALL \
  UnleashedRecomp/tests/test_mod_loader_lzx.cpp
