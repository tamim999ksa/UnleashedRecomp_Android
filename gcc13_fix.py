import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    if not os.path.exists(path): return
    with open(path, 'r') as f: content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(path, 'w') as f: f.write(new_content)

# 1. Anonymous aggregates in xbox.h and xdbf.h
# xbox.h: _XXOVERLAPPED
patch_file('tools/XenonRecomp/XenonUtils/xbox.h',
           r'struct\s+\{\s+be<uint32_t> Error;\s+be<uint32_t> Length;\s+\};',
           'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')

# xdbf.h: XDBFTitleID
patch_file('tools/XenonRecomp/XenonUtils/xdbf.h',
           r'struct\s+\{\s+be<uint16_t> u16;\s+char u8\[0x02\];\s+\};',
           'struct { be<uint16_t> u16; char u8[0x02]; } _s1;')

# 2. Safety update for getOptHeaderPtr
patch_file('tools/XenonRecomp/XenonUtils/xex.h',
           r'inline const void\* getOptHeaderPtr\(const uint8_t\* moduleBytes, uint32_t headerKey\)',
           'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')

# Update call sites for getOptHeaderPtr
# xex.cpp
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'getOptHeaderPtr\(data,', 'getOptHeaderPtr(data, dataSize,')
# xex_patcher.cpp
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(patchBytes,', 'getOptHeaderPtr(patchBytes, patchBytesSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(xexBytes,', 'getOptHeaderPtr(xexBytes, xexBytesSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(outBytes\.data\(\),', 'getOptHeaderPtr(outBytes.data(), outBytes.size(),')

# 3. Explicit return Image() instead of {}
patch_file('tools/XenonRecomp/XenonUtils/image.cpp', r'return \{\};', 'return Image();')
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'return \{\};', 'return Image();')

# 4. Use emplace for SymbolTable to avoid aggregate initialization ambiguity
# We look for image.symbols.insert({ ... })
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp',
           r'image\.symbols\.insert\(\{ (.*?) \}\);',
           r'image.symbols.emplace(\1);')

# 5. Fix NULL pointer arithmetic warnings (NULL used as 0)
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'!= NULL', '!= 0')

# 6. Fix fread unused result warning
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'fread\(temp\.data\(\), 1, fileSize, f\);',
           'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

print("GCC 13 and safety fixes applied.")
