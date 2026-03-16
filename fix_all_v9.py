import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    if not os.path.exists(path):
        print(f"Skipping {path} (not found)")
        return
    with open(path, 'r') as f: content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(path, 'w') as f: f.write(new_content)

# 1. symbol.h - mutable size
patch_file('tools/XenonRecomp/XenonUtils/symbol.h', r'size_t size\{\};', 'mutable size_t size{};')

# 2. symbol_table.h - Robust multi-step range lookup
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
        // Robust range check: iterate backwards to find overlapping symbols that might contain 'address'
        for (int i = 0; i < 32; ++i) {
            if (address >= curr->address && address < curr->address + curr->size) return curr;
            if (address == curr->address) return curr;
            if (curr == begin()) break;
            curr = std::prev(curr);
            // Optimization: if we're too far back, stop
            if (address > curr->address + 1024 * 1024) break;
        }
        return end();
    }

    iterator find(size_t address)
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        for (int i = 0; i < 32; ++i) {
            if (address >= curr->address && address < curr->address + curr->size) return curr;
            if (address == curr->address) return curr;
            if (curr == begin()) break;
            curr = std::prev(curr);
            if (address > curr->address + 1024 * 1024) break;
        }
        return end();
    }
};
""")

# 3. xex.h - getOptHeaderPtr signature safety
patch_file('tools/XenonRecomp/XenonUtils/xex.h',
           r'inline const void\* getOptHeaderPtr\(const uint8_t\* moduleBytes, uint32_t headerKey\)',
           'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')

# 4. xex.cpp - Update calls and emplace
# Pattern: getOptHeaderPtr(data, ... -> getOptHeaderPtr(data, dataSize, ...
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'getOptHeaderPtr\(data,', 'getOptHeaderPtr(data, dataSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'return \{\};', 'return Image();')
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp',
           r'image\.symbols\.insert\(\{ (.*?) \}\);',
           r'image.symbols.emplace(\1);')

# 5. image.cpp - fix return
patch_file('tools/XenonRecomp/XenonUtils/image.cpp', r'return \{\};', 'return Image();')

# 6. xex_patcher.cpp - Update call sites
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(patchBytes,', 'getOptHeaderPtr(patchBytes, patchBytesSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(xexBytes,', 'getOptHeaderPtr(xexBytes, xexBytesSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', r'getOptHeaderPtr\(outBytes\.data\(\),', 'getOptHeaderPtr(outBytes.data(), outBytes.size(),')

# 7. xbox.h & xdbf.h anonymous aggregates
patch_file('tools/XenonRecomp/XenonUtils/xbox.h',
           r'struct\s+\{\s+be<uint32_t> Error;\s+be<uint32_t> Length;\s+\};',
           'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')
patch_file('tools/XenonRecomp/XenonUtils/xdbf.h',
           r'struct\s+\{\s+be<uint16_t> u16;\s+char u8\[0x02\];\s+\};',
           'struct { be<uint16_t> u16; char u8[0x02]; } _s1;')

# 8. function.cpp - tail call fix and min size guarantee
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'r') as f: content = f.read()
# Tail call fix with safety checks
content = content.replace(
    'if (*((uint32_t*)code + 1) == 0x04000048) // shifted ptr tail call',
    'if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048) // shifted ptr tail call'
)
# Inject minimum size guarantee before the final return in Analyze
pos = content.rfind('return fn;')
if pos != -1:
    content = content[:pos] + 'if (fn.size == 0 && data > dataStart) fn.size = 4; ' + content[pos:]
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'w') as f: f.write(content)

# 9. recompiler.cpp
with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'r') as f: content = f.read()
# Reservation
content = content.replace('functions.reserve(8192);', 'functions.reserve(100000);')
# Clears
content = content.replace('out.clear();', 'std::string().swap(out);')
# NULLs
content = content.replace('!= NULL', '!= 0')
# fread
content = content.replace('fread(temp.data(), 1, fileSize, f);', 'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

# Skip logic in Analyse sequential scan
old_scan = """        while (data < dataEnd)
        {
            auto invalidInstr = config.invalidInstructions.find(ByteSwap(*(uint32_t*)data));
            if (invalidInstr != config.invalidInstructions.end())
            {
                base += invalidInstr->second;
                data += invalidInstr->second;
                continue;
            }

            auto& fn = functions.emplace_back(Function::Analyze(data, dataEnd - data, base));
            image.symbols.emplace(fmt::format("sub_{:X}", fn.base), fn.base, fn.size, Symbol_Function);

            size_t advanced = std::max<size_t>(fn.size, 4);
            base += advanced;
            data += advanced;

            if ((base % 0x10000) == 0)
            {
                fmt::println("Scanning section... {:X} / {:X}", base, dataEnd - section.data + section.base);
            }
        }"""

new_scan = """        // Pre-analyze pre-defined symbols to establish baseline ranges
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
                fmt::println("Scanning section... {:X} / {:X} (Found {} functions)", base, section.base + section.size, functions.size());
            }
        }"""
content = content.replace(old_scan, new_scan)
with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'w') as f: f.write(content)

# 10. recompiler_config.cpp NULL fix
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'!= NULL', '!= 0')

print("Comprehensive fix v9 applied.")
