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

# 2. symbol_table.h - Robust range lookup
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

# 3. xex.h - getOptHeaderPtr signature safety
patch_file('tools/XenonRecomp/XenonUtils/xex.h',
           r'inline const void\* getOptHeaderPtr\(const uint8_t\* moduleBytes, uint32_t headerKey\)',
           'inline const void* getOptHeaderPtr(const uint8_t* moduleBytes, size_t moduleSize, uint32_t headerKey)')

# 4. xex.cpp - Pass dataSize, fix emplace, fix return
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'getOptHeaderPtr\(data,', 'getOptHeaderPtr(data, dataSize,')
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp', r'return \{\};', 'return Image();')
patch_file('tools/XenonRecomp/XenonUtils/xex.cpp',
           r'image\.symbols\.insert\(\{ (.*?) \}\);',
           r'image.symbols.emplace(\1);')

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

# 8. function.cpp - Robust Analyze logic
# Completely replace the Analyze function to be safe
analyze_func = """Function Function::Analyze(const void* code, size_t size, size_t base)
{
    Function fn{ base, 0 };

    if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048)
    {
        fn.size = 0x8;
        return fn;
    }

    auto& blocks = fn.blocks;
    blocks.reserve(8);
    blocks.emplace_back();

    const auto* dataStart = (const uint32_t*)code;
    const auto* dataEnd = (const uint32_t*)((const uint8_t*)code + size);
    const uint32_t* data = dataStart;

    std::vector<size_t> blockStack{};
    blockStack.reserve(32);
    blockStack.emplace_back();

    #define RESTORE_DATA() if (!blockStack.empty()) data = (dataStart + ((blocks[blockStack.back()].base + blocks[blockStack.back()].size) / sizeof(*data))) - 1; // continue adds one

    // TODO: Branch fallthrough
    for (; data < dataEnd ; ++data)
    {
        const size_t addr = base + ((data - dataStart) * sizeof(*data));
        if (blockStack.empty())
        {
            break; // it's hideover
        }

        auto& curBlock = blocks[blockStack.back()];
        DEBUG(const auto blockBase = curBlock.base);
        const uint32_t instruction = ByteSwap(*data);

        const uint32_t op = PPC_OP(instruction);
        const uint32_t xop = PPC_XOP(instruction);
        const uint32_t isLink = PPC_BL(instruction); // call

        ppc_insn insn;
        ppc::Disassemble(data, addr, insn);

        // Sanity check
        assert(addr == base + curBlock.base  + curBlock.size);
        if (curBlock.projectedSize != -1 && curBlock.size >= curBlock.projectedSize) // fallthrough
        {
            blockStack.pop_back();
            RESTORE_DATA();
            continue;
        }

        curBlock.size += 4;
        if (op == PPC_OP_BC) // conditional branches all originate from one opcode, thanks RISC
        {
            if (isLink) // just a conditional call, nothing to see here
            {
                continue;
            }

            // TODO: carry projections over to false
            curBlock.projectedSize = -1;
            blockStack.pop_back();

            // TODO: Handle absolute branches?
            assert(!PPC_BA(instruction));
            const size_t branchDest = addr + PPC_BD(instruction);

            // true/false paths
            // left block: false case
            // right block: true case
            const size_t lBase = (addr - base) + 4;
            const size_t rBase = (addr + PPC_BD(instruction)) - base;

            // these will be -1 if it's our first time seeing these blocks
            auto lBlock = fn.SearchBlock(base + lBase);

            if (lBlock == -1)
            {
                blocks.emplace_back(lBase, 0).projectedSize = rBase - lBase;
                lBlock = blocks.size() - 1;

                // push this first, this gets overriden by the true case as it'd be further away
                DEBUG(blocks[lBlock].parent = blockBase);
                blockStack.emplace_back(lBlock);
            }

            size_t rBlock = fn.SearchBlock(base + rBase);
            if (rBlock == -1)
            {
                blocks.emplace_back(branchDest - base, 0);
                rBlock = blocks.size() - 1;

                DEBUG(blocks[rBlock].parent = blockBase);
                blockStack.emplace_back(rBlock);
            }

            RESTORE_DATA();
        }
        else if (op == PPC_OP_B || instruction == 0 || (op == PPC_OP_CTR && (xop == 16 || xop == 528))) // b, blr, end padding
        {
            if (!isLink)
            {
                blockStack.pop_back();

                if (op == PPC_OP_B)
                {
                    assert(!PPC_BA(instruction));
                    const size_t branchDest = addr + PPC_BI(instruction);

                    const size_t branchBase = branchDest - base;
                    const size_t branchBlock = fn.SearchBlock(branchDest);

                    if (branchDest < base)
                    {
                        // Branches before base are just tail calls, no need to chase after those
                        RESTORE_DATA();
                        continue;
                    }

                    // carry over our projection if blocks are next to each other
                    const bool isContinuous = branchBase == curBlock.base + curBlock.size;
                    size_t sizeProjection = (size_t)-1;

                    if (curBlock.projectedSize != -1 && isContinuous)
                    {
                        sizeProjection = curBlock.projectedSize - curBlock.size;
                    }

                    if (branchBlock == -1)
                    {
                        blocks.emplace_back(branchBase, 0, sizeProjection);

                        blockStack.emplace_back(blocks.size() - 1);

                        DEBUG(blocks.back().parent = blockBase);
                        RESTORE_DATA();
                        continue;
                    }
                }
                else if (op == PPC_OP_CTR)
                {
                    // 5th bit of BO tells cpu to ignore the counter, which is a blr/bctr otherwise it's conditional
                    const bool conditional = !(PPC_BO(instruction) & 0x10);
                    if (conditional)
                    {
                        // right block's just going to return
                        const size_t lBase = (addr - base) + 4;
                        size_t lBlock = fn.SearchBlock(lBase);
                        if (lBlock == -1)
                        {
                            blocks.emplace_back(lBase, 0);
                            lBlock = blocks.size() - 1;

                            DEBUG(blocks[lBlock].parent = blockBase);
                            blockStack.emplace_back(lBlock);
                            RESTORE_DATA();
                            continue;
                        }
                    }
                }

                RESTORE_DATA();
            }
        }
        else if (insn.opcode == nullptr)
        {
            blockStack.pop_back();
            RESTORE_DATA();
        }
    }

    // Sort and invalidate discontinuous blocks
    if (blocks.size() > 1)
    {
        std::sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b)
        {
            return a.base < b.base;
        });

        size_t discontinuity = -1;
        for (size_t i = 0; i < blocks.size() - 1; i++)
        {
            if (blocks[i].base + blocks[i].size >= blocks[i + 1].base)
            {
                continue;
            }

            discontinuity = i + 1;
            break;
        }

        if (discontinuity != -1)
        {
            blocks.erase(blocks.begin() + discontinuity, blocks.end());
        }
    }

    fn.size = 0;
    for (const auto& block : blocks)
    {
        // pick the block furthest away
        fn.size = std::max(fn.size, block.base + block.size);
    }

    // Ensure we always return at least 4 bytes if any data was processed to prevent infinite loops
    if (fn.size == 0 && data > dataStart)
    {
        fn.size = 4;
    }

    return fn;
}"""
patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'Function Function::Analyze\(const void\* code, size_t size, size_t base\)\s+\{.*?\}',
           analyze_func)

# 9. recompiler.cpp
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'functions\.reserve\(8192\);', 'functions.reserve(50000);')

# Analyse sequential scan logic
analyse_logic = """        // Pre-analyze pre-defined symbols to establish baseline ranges
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
           analyse_logic)

# SaveCurrentOutData optimization
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'out\.clear\(\);', 'std::string().swap(out);')

# NULL warnings
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'!= NULL', '!= 0')

# fread warning
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'fread\(temp\.data\(\), 1, fileSize, f\);',
           'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

print("Comprehensive patching complete.")
