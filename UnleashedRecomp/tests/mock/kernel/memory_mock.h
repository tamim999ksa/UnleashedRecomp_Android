#pragma once
#include <cstdint>
#include <cstddef>
#include <map>

struct Memory
{
    uint8_t* base;
    std::map<size_t, void*> mapped_addresses;

    void* Translate(size_t offset) const noexcept
    {
        if (mapped_addresses.count(offset))
            return mapped_addresses.at(offset);
        return base + offset;
    }

    // Add dummy members to satisfy interface if needed, but achievement test mainly uses mapped_addresses
    bool IsInMemoryRange(const void* ptr) const { return true; }
    uint32_t MapVirtual(void* ptr) { return (uint32_t)(uintptr_t)ptr; }
};
extern Memory g_memory;
