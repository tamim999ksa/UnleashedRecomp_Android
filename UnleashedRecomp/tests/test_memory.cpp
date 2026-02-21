#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <vector>
#include <iostream>
#include <cstdlib>

// Define dependencies for Memory struct
#define PPC_MEMORY_SIZE 0x10000 // 64KB for testing
using PPCFunc = void(void*);
#define PPC_LOOKUP_FUNC(base, guest) (*(PPCFunc**)((uint8_t*)base + guest))

// Include target header
#include "../kernel/memory.h"

// Global memory instance
Memory g_memory;

// Mock MmGetHostAddress
extern "C" void* MmGetHostAddress(uint32_t ptr) {
    return g_memory.Translate(ptr);
}

// Implement Memory constructor for test environment
Memory::Memory() {
    // Allocate buffer with padding
    static std::vector<uint8_t> buffer(PPC_MEMORY_SIZE + 4096);
    base = buffer.data();
}

TEST_CASE("Memory::IsInMemoryRange") {
    uint8_t* start = g_memory.base;
    uint8_t* end = start + PPC_MEMORY_SIZE;

    SUBCASE("Valid ranges") {
        CHECK(g_memory.IsInMemoryRange(start));
        CHECK(g_memory.IsInMemoryRange(start + 100));
        CHECK(g_memory.IsInMemoryRange(end - 1));
    }

    SUBCASE("Invalid ranges") {
        CHECK_FALSE(g_memory.IsInMemoryRange(start - 1));
        CHECK_FALSE(g_memory.IsInMemoryRange(end));
        CHECK_FALSE(g_memory.IsInMemoryRange(nullptr));
    }
}

TEST_CASE("Memory::Translate") {
    uint8_t* start = g_memory.base;

    SUBCASE("Valid translations") {
        CHECK(g_memory.Translate(0) == start);
        CHECK(g_memory.Translate(100) == start + 100);
        CHECK(g_memory.Translate(PPC_MEMORY_SIZE - 1) == start + PPC_MEMORY_SIZE - 1);
    }
}

TEST_CASE("Memory::MapVirtual") {
    uint8_t* start = g_memory.base;

    SUBCASE("Valid mappings") {
        CHECK(g_memory.MapVirtual(start) == 0);
        CHECK(g_memory.MapVirtual(start + 100) == 100);
        CHECK(g_memory.MapVirtual(start + PPC_MEMORY_SIZE - 1) == PPC_MEMORY_SIZE - 1);
    }
}

// Dummy function for testing function lookup
void DummyFunc(void*) {}

TEST_CASE("Memory::FunctionLookup") {
    uint32_t funcOffset = 0x100;

    // Insert function
    g_memory.InsertFunction(funcOffset, DummyFunc);

    // Find function
    CHECK(g_memory.FindFunction(funcOffset) == DummyFunc);

    // Check another offset is null (assuming initialized to 0)
    CHECK(g_memory.FindFunction(funcOffset + sizeof(void*)) == nullptr);
}
