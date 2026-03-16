import os
import re

def fix_file(path, replacements):
    if not os.path.exists(path): return
    with open(path, 'r') as f: content = f.read()
    for old, new in replacements:
        content = content.replace(old, new)
    with open(path, 'w') as f: f.write(content)

# Logic: Symbol range lookup
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
        for (int i = 0; i < 32; ++i) {
            if (address >= curr->address && address < curr->address + curr->size) return curr;
            if (address == curr->address) return curr;
            if (curr == begin()) break;
            curr = std::prev(curr);
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

# Logic: Memory and Scan progress
with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'r') as f: content = f.read()
content = content.replace('functions.reserve(8192);', 'functions.reserve(100000);')
content = content.replace('out.clear();', 'std::string().swap(out);')
content = content.replace('!= NULL', '!= 0')
content = content.replace('fread(temp.data(), 1, fileSize, f);', 'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

old_loop = """        while (data < dataEnd)
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

new_loop = """        for (auto it = image.symbols.lower_bound(section.base); it != image.symbols.end() && it->address < section.base + section.size; ++it)
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
content = content.replace(old_loop, new_loop)
with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'w') as f: f.write(content)

# Compatibility Fixes
fix_file('tools/XenonRecomp/XenonUtils/symbol.h', [('size_t size{};', 'mutable size_t size{};')])
fix_file('tools/XenonRecomp/XenonUtils/image.cpp', [('return {};', 'return Image();')])
fix_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', [('!= NULL', '!= 0')])
fix_file('tools/XenonRecomp/XenonUtils/xex.h', [('inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, uint32_t headerKey)', 'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')])
fix_file('tools/XenonRecomp/XenonUtils/xex.cpp', [('getOptHeaderPtr(data,', 'getOptHeaderPtr(data, dataSize,'), ('image.symbols.insert({ name->second, descriptors[im].firstThunk, sizeof(thunk), Symbol_Function });', 'image.symbols.emplace(name->second, descriptors[im].firstThunk, (size_t)sizeof(thunk), Symbol_Function);'), ('return {};', 'return Image();')])
fix_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', [('getOptHeaderPtr(patchBytes, XEX_HEADER_DELTA_PATCH_DESCRIPTOR)', 'getOptHeaderPtr(patchBytes, patchBytesSize, XEX_HEADER_DELTA_PATCH_DESCRIPTOR)'), ('getOptHeaderPtr(patchBytes, XEX_HEADER_FILE_FORMAT_INFO)', 'getOptHeaderPtr(patchBytes, patchBytesSize, XEX_HEADER_FILE_FORMAT_INFO)'), ('getOptHeaderPtr(xexBytes, XEX_HEADER_FILE_FORMAT_INFO)', 'getOptHeaderPtr(xexBytes, xexBytesSize, XEX_HEADER_FILE_FORMAT_INFO)'), ('getOptHeaderPtr(outBytes.data(), XEX_HEADER_FILE_FORMAT_INFO)', 'getOptHeaderPtr(outBytes.data(), outBytes.size(), XEX_HEADER_FILE_FORMAT_INFO)')])
fix_file('tools/XenonRecomp/XenonUtils/xbox.h', [('struct\n        {\n            be<uint32_t> Error;\n            be<uint32_t> Length;\n        };', 'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')])
fix_file('tools/XenonRecomp/XenonUtils/xdbf.h', [('struct\n    {\n        be<uint16_t> u16;\n        char u8[0x02];\n    };', 'struct { be<uint16_t> u16; char u8[0x02]; } _s1;')])

# Function Analyze min size
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'r') as f: content = f.read()
content = content.replace('if (*((uint32_t*)code + 1) == 0x04000048) // shifted ptr tail call', 'if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048) // shifted ptr tail call')
pos = content.rfind('return fn;')
if pos != -1:
    content = content[:pos] + 'if (fn.size == 0 && data > (const uint32_t*)code) fn.size = 4; ' + content[pos:]
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'w') as f: f.write(content)

print("Surgical fixes complete.")
