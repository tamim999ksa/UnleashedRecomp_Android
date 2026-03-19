#include "mmio.h"
#include <cstdio>

extern "C" {

uint8_t MMIO_Read8(uint32_t addr, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Read8 from 0x%08X at PC 0x%016llX\n", addr, (unsigned long long)pc);
    return 0;
}

uint16_t MMIO_Read16(uint32_t addr, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Read16 from 0x%08X at PC 0x%016llX\n", addr, (unsigned long long)pc);
    return 0;
}

uint32_t MMIO_Read32(uint32_t addr, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Read32 from 0x%08X at PC 0x%016llX\n", addr, (unsigned long long)pc);
    return 0;
}

uint64_t MMIO_Read64(uint32_t addr, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Read64 from 0x%08X at PC 0x%016llX\n", addr, (unsigned long long)pc);
    return 0;
}

void MMIO_Write8(uint32_t addr, uint8_t val, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Write8 to 0x%08X: 0x%02X at PC 0x%016llX\n", addr, val, (unsigned long long)pc);
}

void MMIO_Write16(uint32_t addr, uint16_t val, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Write16 to 0x%08X: 0x%04X at PC 0x%016llX\n", addr, val, (unsigned long long)pc);
}

void MMIO_Write32(uint32_t addr, uint32_t val, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Write32 to 0x%08X: 0x%08X at PC 0x%016llX\n", addr, val, (unsigned long long)pc);
}

void MMIO_Write64(uint32_t addr, uint64_t val, uint64_t pc)
{
    fprintf(stderr, "[MMIO] Write64 to 0x%08X: 0x%016llX at PC 0x%016llX\n", addr, (unsigned long long)val, (unsigned long long)pc);
}

}
