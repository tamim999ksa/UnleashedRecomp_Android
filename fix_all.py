import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    if not os.path.exists(path): return
    with open(path, 'r') as f: content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(path, 'w') as f: f.write(new_content)

# 1. symbol.h - mutable size
patch_file('tools/XenonRecomp/XenonUtils/symbol.h', r'size_t size\{\};', 'mutable size_t size{};')

# 2. symbol_table.h - range lookup (completely rewrite)
with open('tools/XenonRecomp/XenonUtils/symbol_table.h', 'w') as f:
    f.write("""#pragma once
#include "symbol.h"
#include <set>
#include <iterator>

class SymbolTable : public std::multiset<Symbol, SymbolComparer>
{
public:
    const_iterator find(size_t address) const
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        if (address >= curr->address && address < curr->address + curr->size) return curr;
        if (address == curr->address) return curr;
        return end();
    }

    iterator find(size_t address)
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        if (address >= curr->address && address < curr->address + curr->size) return curr;
        if (address == curr->address) return curr;
        return end();
    }
};""")

# 3. xex.h - getOptHeaderPtr signature
patch_file('tools/XenonRecomp/XenonUtils/xex.h',
           r'inline const void\* getOptHeaderPtr\(const uint8_t\* moduleBytes, uint32_t headerKey\)',
           'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')

# 4. xex.cpp - getOptHeaderPtr calls and emplace
# Update getOptHeaderPtr(data, ... to getOptHeaderPtr(data, dataSize, ...
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'getOptHeaderPtr\(data,', 'getOptHeaderPtr(data, dataSize,')

# Fix image.symbols.insert({...}) to image.symbols.emplace(...)
# Original: image.symbols.insert({ name->second, descriptors[im].firstThunk, sizeof(thunk), Symbol_Function });
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp',
           r'image\.symbols\.insert\(\{(.*?)\}\);',
           r'image.symbols.emplace(\1);')

# Fix aggregate return Image()
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'return \{\};', 'return Image();')
patch_file('tools/XenonRecomp/XenonUtils/image.cpp', r'return \{\};', 'return Image();')

# 5. xex_patcher.cpp - getOptHeaderPtr calls
# apply(..., const uint8_t* patchBytes, size_t patchBytesSize, ...)
# apply(const uint8_t* xexBytes, size_t xexBytesSize, ...)
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(patchBytes,', 'getOptHeaderPtr(patchBytes, patchBytesSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(xexBytes,', 'getOptHeaderPtr(xexBytes, xexBytesSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(outBytes\.data\(\),', 'getOptHeaderPtr(outBytes.data(), outBytes.size(),')

# 6. xbox.h & xdbf.h anonymous aggregates
patch_file('tools/XenonRecomp/XenonUtils/xbox.h',
           r'struct\s+\{\s+be<uint32_t> Error;\s+be<uint32_t> Length;\s+\};',
           'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')
patch_file('tools/XenonRecomp/XenonUtils/xdbf.h',
           r'struct\s+\{\s+be<uint16_t> u16;\s+char u8\[0x02\];\s+\};',
           'struct { be<uint16_t> u16; char u8[0x02]; } _s1;')

# 7. function.cpp - tail call check and min size guarantee
# We find the existing tail call check and wrap it with size check.
# Also add min size at the end.
patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'if \(\*\(\(uint32_t\*\)code \+ 1\) == 0x04000048\)',
           'const uint32_t* dataStart = (const uint32_t*)code;\n    if (size >= 8 && dataStart[1] == 0x48000004)')

patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'return fn;\s+\}',
           '    if (fn.size == 0 && data > dataStart) fn.size = 4;\n    return fn;\n}')

# 8. recompiler.cpp
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'functions\.reserve\(8192\);', 'functions.reserve(50000);')

analyse_logic = """    for (auto& section : image.sections)
    {
        if ((section.flags & SectionFlags_Code) == 0)
            continue;

        auto* data = section.data;
        auto* dataEnd = section.data + section.size;
        auto base = section.base;

        for (auto it = image.symbols.lower_bound(section.base); it != image.symbols.end() && it->address < section.base + section.size; ++it)
        {
            if (it->type == Symbol_Function)
            {
                auto data_ptr = section.data + it->address - section.base;
                auto& fn = functions.emplace_back(Function::Analyze(data_ptr, section.base + section.size - it->address, it->address));
                it->size = fn.size;
            }
        }

        while (data < dataEnd)
        {
            auto invalidInstr = config.invalidInstructions.find(ByteSwap(*(uint32_t*)data));
            if (invalidInstr != config.invalidInstructions.end())
            {
                base += invalidInstr->second;
                data += invalidInstr->second;
                continue;
            }

            auto fnSymbol = image.symbols.find(base);
            if (fnSymbol != image.symbols.end() && fnSymbol->type == Symbol_Function)
            {
                size_t skipSize = std::max<size_t>((fnSymbol->address + fnSymbol->size) - base, 4);
                base += skipSize;
                data += skipSize;
            }
            else
            {
                auto& fn = functions.emplace_back(Function::Analyze(data, dataEnd - data, base));
                image.symbols.emplace(fmt::format("sub_{:X}", fn.base), fn.base, fn.size, Symbol_Function);
                size_t advanced = std::max<size_t>(fn.size, 4);
                base += advanced;
                data += advanced;
            }

            if ((base % 0x10000) == 0)
            {
                fmt::println("Scanning section... {:X} / {:X}", base, section.base + section.size);
            }
        }
    }"""
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'for \(auto& section : image\.sections\).*?fmt::println\("Analysis complete\. Found \{\} functions\.", functions\.size\(\)\);\s+\}',
           analyse_logic + '\n\n    std::sort(functions.begin(), functions.end(), [](auto& lhs, auto& rhs) { return lhs.base < rhs.base; });\n    fmt::println("Analysis complete. Found {} functions.", functions.size());\n}')

patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'out\.clear\(\);', 'std::string().swap(out);')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'fread\(temp\.data\(\), 1, fileSize, f\);',
           'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

print("Patching complete.")
