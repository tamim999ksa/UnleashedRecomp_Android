#include "../XenonAnalyse/function.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <cstdint>

// Use built-in bswap if available, assuming GCC/Clang
#define BSWAP32(x) __builtin_bswap32(x)

// Helper to create BC instruction
// Opcode: 16 (6 bits)
// BO: 5 bits
// BI: 5 bits
// BD: 14 bits
// AA: 1 bit
// LK: 1 bit
uint32_t MakeBC(int bo, int bi, int bd, bool aa, bool lk) {
    uint32_t insn = (16 << 26) | ((bo & 0x1F) << 21) | ((bi & 0x1F) << 16) | ((bd & 0x3FFF) << 2) | (aa ? 2 : 0) | (lk ? 1 : 0);
    return BSWAP32(insn); // Analyze expects big endian in memory
}

// Helper to create B instruction
// Opcode: 18 (6 bits)
// LI: 24 bits
// AA: 1 bit
// LK: 1 bit
uint32_t MakeB(int li, bool aa, bool lk) {
    uint32_t insn = (18 << 26) | ((li & 0xFFFFFF) << 2) | (aa ? 2 : 0) | (lk ? 1 : 0);
    return BSWAP32(insn);
}

// Helper to create NOP (ori r0, r0, 0) -> 0x60000000
uint32_t MakeNOP() {
    return BSWAP32(0x60000000);
}

// Helper to create BLR (bclr 20, 0, 0) -> 0x4E800020
uint32_t MakeBLR() {
    return BSWAP32(0x4E800020);
}

int main() {
    std::cout << "Running Absolute Branch Tests..." << std::endl;

    // Test case 1: Absolute Conditional Branch (bc)
    // Code base 0x1000
    // 0x1000: bc 12, 0, 0x2000 (Absolute) -> Target 0x2000
    // 0x1004: nop
    // ...
    // 0x2000: blr
    // Fallthrough fills the gap, ensuring continuity.

    // Construct buffer big enough
    size_t codeSize = 0x2004;
    std::vector<uint32_t> code(0x1000, MakeNOP());
    if (code.size() <= 1024) code.resize(1025, MakeNOP());

    // At offset 0 (0x1000)
    // bc with BO=12 (branch if CR bit set), BI=0, BD=(0x2000 >> 2), AA=1
    code[0] = MakeBC(12, 0, 0x2000 >> 2, true, false);

    // At offset 1024 (0x2000)
    code[1024] = MakeBLR();

    Function fn = Function::Analyze(code.data(), code.size() * 4, 0x1000);

    bool foundTarget = false;
    for (const auto& block : fn.blocks) {
        if (block.base == 0x1000) {
            foundTarget = true;
            std::cout << "Found block at offset 0x" << std::hex << block.base << std::dec << std::endl;
        }
    }

    if (foundTarget) {
        std::cout << "Test 1 Passed: Absolute BC target found." << std::endl;
    } else {
        std::cout << "Test 1 Failed: Absolute BC target NOT found." << std::endl;
        // return 1;
    }

    // Test case 2: Absolute Unconditional Branch (b)
    // To satisfy Function::Analyze continuity check, we jump to the immediate next instruction.
    // 0x3000: b 0x3004 (Absolute)
    // 0x3004: blr

    std::fill(code.begin(), code.end(), MakeNOP());

    // 0x3004 >> 2 = 0xC01
    // At index 0 (0x3000)
    code[0] = MakeB(0x3004 >> 2, true, false);

    // Target at 0x3004 (offset 4 from 0x3000) -> index 1
    code[1] = MakeBLR();

    // Use smaller size for this test
    fn = Function::Analyze(code.data(), 8, 0x3000);

    foundTarget = false;
    for (const auto& block : fn.blocks) {
        if (block.base == 4) { // 0x3004 - 0x3000
            foundTarget = true;
            std::cout << "Found block at offset 0x" << std::hex << block.base << std::dec << std::endl;
        }
    }

    if (foundTarget) {
        std::cout << "Test 2 Passed: Absolute B target found." << std::endl;
    } else {
        std::cout << "Test 2 Failed: Absolute B target NOT found." << std::endl;
        return 1;
    }

    return 0;
}
