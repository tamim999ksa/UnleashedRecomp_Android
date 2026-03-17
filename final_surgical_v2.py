import os
import re

ROOT = '/app'

def get_content(rel_path):
    path = os.path.join(ROOT, 'tools', rel_path)
    with open(path, 'r') as f:
        return f.read()

def write_override(rel_path, content):
    path = os.path.join(ROOT, 'build_overrides/tools', rel_path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(content)

# 1. XenonUtils/symbol.h
content = get_content('XenonRecomp/XenonUtils/symbol.h')
content = content.replace('size_t size{};', 'mutable size_t size{};')
write_override('XenonRecomp/XenonUtils/symbol.h', content)

# 2. XenonUtils/symbol_table.h
symbol_table = """#pragma once
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
"""
write_override('XenonRecomp/XenonUtils/symbol_table.h', symbol_table)

# 3. XenonUtils/xbox.h
content = get_content('XenonRecomp/XenonUtils/xbox.h')
content = content.replace(
    '        struct\n        {\n            be<uint32_t> Error;\n            be<uint32_t> Length;\n        };',
    '        struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;'
)
write_override('XenonRecomp/XenonUtils/xbox.h', content)

# 4. XenonUtils/xdbf.h & wrapper
content = get_content('XenonRecomp/XenonUtils/xdbf.h')
content = content.replace(
    '    struct\n    {\n        be<uint16_t> u16;\n        char u8[0x02];\n    };',
    '    struct { be<uint16_t> u16; char u8[0x02]; } _s1;'
)
write_override('XenonRecomp/XenonUtils/xdbf.h', content)

content = get_content('XenonRecomp/XenonUtils/xdbf_wrapper.cpp')
content = content.replace('block.TitleID.u16 = titleID.u16;', 'block.TitleID._s1.u16 = titleID._s1.u16;')
write_override('XenonRecomp/XenonUtils/xdbf_wrapper.cpp', content)

# 5. Safe getOptHeaderPtr
content = get_content('XenonRecomp/XenonUtils/xex.h')
content = content.replace('inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, uint32_t headerKey)',
                          'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')
write_override('XenonRecomp/XenonUtils/xex.h', content)

content = get_content('XenonRecomp/XenonUtils/xex.cpp')
content = content.replace('getOptHeaderPtr(data,', 'getOptHeaderPtr(data, dataSize,')
content = content.replace('return {};', 'return Image();')
content = content.replace('image.symbols.insert({ name->second, descriptors[im].firstThunk, sizeof(thunk), Symbol_Function });',
                          'image.symbols.emplace(name->second, descriptors[im].firstThunk, (size_t)sizeof(thunk), Symbol_Function);')
write_override('XenonRecomp/XenonUtils/xex.cpp', content)

content = get_content('XenonRecomp/XenonUtils/xex_patcher.cpp')
content = content.replace('getOptHeaderPtr(patchBytes,', 'getOptHeaderPtr(patchBytes, patchBytesSize,')
content = content.replace('getOptHeaderPtr(xexBytes,', 'getOptHeaderPtr(xexBytes, xexBytesSize,')
content = content.replace('getOptHeaderPtr(outBytes.data(),', 'getOptHeaderPtr(outBytes.data(), outBytes.size(),')
write_override('XenonRecomp/XenonUtils/xex_patcher.cpp', content)

# 6. image.cpp
content = get_content('XenonRecomp/XenonUtils/image.cpp')
content = content.replace('return {};', 'return Image();')
write_override('XenonRecomp/XenonUtils/image.cpp', content)

# 7. XenonAnalyse/function.cpp
content = get_content('XenonRecomp/XenonAnalyse/function.cpp')
content = content.replace('if (*((uint32_t*)code + 1) == 0x04000048) // shifted ptr tail call',
                          'if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048) // shifted ptr tail call')
pos = content.rfind('return fn;')
if pos != -1:
    content = content[:pos] + 'if (fn.size == 0 && data > (const uint32_t*)code) fn.size = 4; ' + content[pos:]
write_override('XenonRecomp/XenonAnalyse/function.cpp', content)

# 8. XenonRecomp/recompiler.cpp
content = get_content('XenonRecomp/XenonRecomp/recompiler.cpp')
content = content.replace('functions.reserve(8192);', 'functions.reserve(100000);')
content = content.replace('out.clear();', 'std::string().swap(out);')
content = content.replace('!= NULL', '!= 0')
content = content.replace('fread(temp.data(), 1, fileSize, f);', 'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

# The original code has establishment logic spread out. We surgically inject skip logic into the sequential scan.
new_loop = """        // Establish baseline ranges
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
# Identify the start of while loop and the setup of data and base
pattern = r'size_t base = section\.base;.*?while \(data < dataEnd\)\s+\{.*?base \+= fn\.size;\s+data \+= fn\.size;\s+\}'
content = re.sub(pattern, 'size_t base = section.base;\n' + new_loop, content, flags=re.DOTALL)
write_override('XenonRecomp/XenonRecomp/recompiler.cpp', content)

content = get_content('XenonRecomp/XenonRecomp/recompiler_config.cpp')
content = content.replace('!= NULL', '!= 0')
write_override('XenonRecomp/XenonRecomp/recompiler_config.cpp', content)

# 9. XenosRecomp fixes
content = get_content('XenosRecomp/XenosRecomp/main.cpp')
content = content.replace('fread(data.get(), 1, fileSize, file);', 'if (fread(data.get(), 1, fileSize, file) != fileSize) { fclose(file); return nullptr; }')
write_override('XenosRecomp/XenosRecomp/main.cpp', content)

content = get_content('XenosRecomp/XenosRecomp/shader_recompiler.cpp')
content = content.replace('if (shaderContainer->definitionTableOffset != NULL)', 'if (shaderContainer->definitionTableOffset != 0)')

# Int4 anonymous struct fix
content = content.replace(
    '                    struct\n                    {\n                        int8_t x;\n                        int8_t y;\n                        int8_t z;\n                        int8_t w;\n                    };',
    '                    struct { int8_t x, y, z, w; } s;'
)
content = content.replace('x, y, z, w);', 's.x, s.y, s.z, s.w);')

# CF anonymous union/struct fix
old_cf = """    union
    {
        ControlFlowInstruction controlFlow[2];
        struct
        {
            uint32_t code0;
            uint32_t code1;
            uint32_t code2;
            uint32_t code3;
        };
    };"""
new_cf = """    union {
        ControlFlowInstruction controlFlow[2];
        struct { uint32_t code0, code1, code2, code3; } s;
    } u;"""
content = content.replace(old_cf, new_cf)
content = content.replace('code0 = controlFlowCode[0];', 'u.s.code0 = controlFlowCode[0];')
content = content.replace('code1 = controlFlowCode[1] & 0xFFFF;', 'u.s.code1 = controlFlowCode[1] & 0xFFFF;')
content = content.replace('code2 = (controlFlowCode[1] >> 16) | (controlFlowCode[2] << 16);', 'u.s.code2 = (controlFlowCode[1] >> 16) | (controlFlowCode[2] << 16);')
content = content.replace('code3 = controlFlowCode[2] >> 16;', 'u.s.code3 = controlFlowCode[2] >> 16;')
content = content.replace('for (auto& cfInstr : controlFlow)', 'for (auto& cfInstr : u.controlFlow)')

# ALU anonymous union/struct fix
old_alu = """                union
                {
                    VertexFetchInstruction vertexFetch;
                    TextureFetchInstruction textureFetch;
                    AluInstruction alu;
                    struct
                    {
                        uint32_t code0;
                        uint32_t code1;
                        uint32_t code2;
                    };
                };"""
new_alu = """                union {
                    VertexFetchInstruction vertexFetch;
                    TextureFetchInstruction textureFetch;
                    AluInstruction alu;
                    struct { uint32_t code0, code1, code2; } s;
                } u;"""
content = content.replace(old_alu, new_alu)
content = content.replace('code0 = instructionCode[0];', 'u.s.code0 = instructionCode[0];')
content = content.replace('code1 = instructionCode[1];', 'u.s.code1 = instructionCode[1];')
content = content.replace('code2 = instructionCode[2];', 'u.s.code2 = instructionCode[2];')
content = content.replace('if (vertexFetch.opcode', 'if (u.vertexFetch.opcode')
content = content.replace('else if (textureFetch.opcode', 'else if (u.textureFetch.opcode')
content = content.replace('else if (alu.opcode', 'else if (u.alu.opcode')

write_override('XenosRecomp/XenosRecomp/shader_recompiler.cpp', content)

print("Final surgical v2 complete.")
