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

# 2. symbol_table.h - range lookup
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
};
""")

# 3. xex.h - getOptHeaderPtr signature
patch_file('tools/XenonRecomp/XenonUtils/xex.h',
           r'inline const void\* getOptHeaderPtr\(const uint8_t\* moduleBytes, uint32_t headerKey\)',
           'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')

# 4. xex.cpp - Pass dataSize, fix emplace, fix return
# getOptHeaderPtr(data, ... -> getOptHeaderPtr(data, dataSize, ...
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'getOptHeaderPtr\(data,', 'getOptHeaderPtr(data, dataSize,')
# image.symbols.insert({ ... }) -> image.symbols.emplace(...)
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp',
           r'image\.symbols\.insert\(\{ (.*?) \}\);',
           r'image.symbols.emplace(\1);')
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'return \{\};', 'return Image();')

# 5. image.cpp - fix return
patch_file('tools/XenonRecomp/XenonUtils/image.cpp', r'return \{\};', 'return Image();')

# 6. xex_patcher.cpp - getOptHeaderPtr calls
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

# 8. function.cpp - tail call check and min size guarantee
patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'if \(\*\(\(uint32_t\*\)code \+ 1\) == 0x04000048\)\s+\{.*?\}',
           'if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048) { fn.size = 0x8; return fn; }')

# Add min size guarantee at end of Function::Analyze
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'r') as f: content = f.read()
# Find the end of Analyze and inject before the final return
pos = content.rfind('return fn;')
if pos != -1:
    content = content[:pos] + 'if (fn.size == 0 && data > dataStart) fn.size = 4; ' + content[pos:]
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'w') as f: f.write(content)

# 9. recompiler.cpp
# Reservation
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'functions\.reserve\(8192\);', 'functions.reserve(50000);')

# skip logic in Analyse
analyse_scan_logic = """        // Pre-analyze pre-defined symbols to establish baseline ranges
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
        }"""
# Find the start of the while loop and replace it and the preceding data = section.data
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'while \(data < dataEnd\).*?base \+= advanced;\s+data \+= advanced;\s+\}',
           analyse_scan_logic)

# SaveCurrentOutData optimization
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'out\.clear\(\);', 'std::string().swap(out);')

# NULL warnings
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'!= NULL', '!= 0')

# fread warning
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'fread\(temp\.data\(\), 1, fileSize, f\);',
           'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

print("Final patch v8 applied.")
