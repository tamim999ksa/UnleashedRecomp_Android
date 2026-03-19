#pragma once
#include <cstdint>

inline bool IsMMIO(uint32_t addr)
{
    // MMIO range: 0x7F000000 - 0x7FFFFFFF
    return (addr & 0xFF000000) == 0x7F000000;
}

extern "C" uint8_t MMIO_Read8(uint32_t addr, uint64_t pc);
extern "C" uint16_t MMIO_Read16(uint32_t addr, uint64_t pc);
extern "C" uint32_t MMIO_Read32(uint32_t addr, uint64_t pc);
extern "C" uint64_t MMIO_Read64(uint32_t addr, uint64_t pc);

extern "C" void MMIO_Write8(uint32_t addr, uint8_t val, uint64_t pc);
extern "C" void MMIO_Write16(uint32_t addr, uint16_t val, uint64_t pc);
extern "C" void MMIO_Write32(uint32_t addr, uint32_t val, uint64_t pc);
extern "C" void MMIO_Write64(uint32_t addr, uint64_t val, uint64_t pc);
