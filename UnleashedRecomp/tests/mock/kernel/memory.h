#pragma once
#include <map>
#include <cstdint>

struct Memory {
    std::map<uint32_t, void*> mapped_addresses;

    void* Translate(uint32_t address) {
        if (mapped_addresses.find(address) != mapped_addresses.end()) {
            return mapped_addresses[address];
        }
        return nullptr;
    }
};

extern Memory g_memory;
