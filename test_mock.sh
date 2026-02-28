#!/bin/bash
g++ -fsyntax-only -I UnleashedRecomp/tests/mock -I UnleashedRecomp -I thirdparty -I tools/XenonRecomp/XenonUtils -I UnleashedRecomp/api -I thirdparty/unordered_dense/include -I tools/XenonRecomp/thirdparty/fmt/include -I tools/XenonRecomp/thirdparty/tomlplusplus/include -I thirdparty/xxHash -I thirdparty/SDL/include UnleashedRecomp/mod/mod_loader.cpp
