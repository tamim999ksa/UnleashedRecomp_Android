import os
import re

def fix_file(path, replacements):
    with open(path, 'r') as f:
        content = f.read()
    for old, new in replacements:
        content = content.replace(old, new)
    with open(path, 'w') as f:
        f.write(content)

# 1. symbol.h
fix_file('tools/XenonRecomp/XenonUtils/symbol.h', [('size_t size{};', 'mutable size_t size{};')])

# 2. symbol_table.h
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

# 3. xex.h
fix_file('tools/XenonRecomp/XenonUtils/xex.h', [
    ('inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, uint32_t headerKey)',
     'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')
])

# 4. xex.cpp
fix_file('tools/XenonRecomp/XenonUtils/xex.cpp', [
    ('getOptHeaderPtr(data,', 'getOptHeaderPtr(data, dataSize,'),
    ('image.symbols.insert({ name->second, descriptors[im].firstThunk, sizeof(thunk), Symbol_Function });',
     'image.symbols.emplace(name->second, descriptors[im].firstThunk, (size_t)sizeof(thunk), Symbol_Function);'),
    ('return {};', 'return Image();')
])

# 5. image.cpp
fix_file('tools/XenonRecomp/XenonUtils/image.cpp', [('return {};', 'return Image();')])

# 6. xex_patcher.cpp
fix_file('tools/XenonRecomp/XenonUtils/xex_patcher.cpp', [
    ('getOptHeaderPtr(patchBytes, XEX_HEADER_DELTA_PATCH_DESCRIPTOR)', 'getOptHeaderPtr(patchBytes, patchBytesSize, XEX_HEADER_DELTA_PATCH_DESCRIPTOR)'),
    ('getOptHeaderPtr(patchBytes, XEX_HEADER_FILE_FORMAT_INFO)', 'getOptHeaderPtr(patchBytes, patchBytesSize, XEX_HEADER_FILE_FORMAT_INFO)'),
    ('getOptHeaderPtr(xexBytes, XEX_HEADER_FILE_FORMAT_INFO)', 'getOptHeaderPtr(xexBytes, xexBytesSize, XEX_HEADER_FILE_FORMAT_INFO)'),
    ('getOptHeaderPtr(outBytes.data(), XEX_HEADER_FILE_FORMAT_INFO)', 'getOptHeaderPtr(outBytes.data(), outBytes.size(), XEX_HEADER_FILE_FORMAT_INFO)')
])

# 7. xbox.h
fix_file('tools/XenonRecomp/XenonUtils/xbox.h', [
    ("""        struct
        {
            be<uint32_t> Error;
            be<uint32_t> Length;
        };""",
     """        struct
        {
            be<uint32_t> Error;
            be<uint32_t> Length;
        } _s1;""")
])

# 8. xdbf.h
fix_file('tools/XenonRecomp/XenonUtils/xdbf.h', [
    ("""    struct
    {
        be<uint16_t> u16;
        char u8[0x02];
    };""",
     """    struct
    {
        be<uint16_t> u16;
        char u8[0x02];
    } _s1;""")
])

# 9. function.cpp
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'r') as f:
    content = f.read()

# Tail call fix
content = content.replace(
    'if (*((uint32_t*)code + 1) == 0x04000048) // shifted ptr tail call',
    'if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048) // shifted ptr tail call'
)

# Min size fix at the end of Function::Analyze
# We look for the last return fn; and inject before it
pos = content.rfind('return fn;')
if pos != -1:
    content = content[:pos] + 'if (fn.size == 0 && data > dataStart) fn.size = 4; ' + content[pos:]

with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'w') as f:
    f.write(content)

# 10. recompiler.cpp
with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'r') as f:
    content = f.read()

content = content.replace('functions.reserve(8192);', 'functions.reserve(50000);')

# Sequential scan skip logic
# We find the section loop and the while loop inside it
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

new_loop = """        // Pre-analyze pre-defined symbols to establish baseline ranges
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

content = content.replace(old_loop, new_loop)

# Memory and other minor fixes
content = content.replace('out.clear();', 'std::string().swap(out);')
content = content.replace('!= NULL', '!= 0')
content = content.replace('fread(temp.data(), 1, fileSize, f);', 'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'w') as f:
    f.write(content)

# recompiler_config.cpp NULL fix
fix_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', [('!= NULL', '!= 0')])
