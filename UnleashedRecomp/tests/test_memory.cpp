#include <cstdint>
#include <cassert>
#include <vector>
#include <iostream>
#include <cstdlib>

// Define dependencies for Memory struct
#define PPC_MEMORY_SIZE 0x10000 // 64KB for testing
using PPCFunc = void(void*);
#define PPC_LOOKUP_FUNC(base, guest) (*(PPCFunc**)((uint8_t*)base + guest)) // Simplified mock if needed, or just nullptr

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
    // Allocate buffer
    // We use malloc to ensure alignment or just new
    // But since base is uint8_t*, we can just point to a static buffer or heap allocated one.
    // However, g_memory is global, so constructor runs before main.
    // Ideally we want to control base in tests.

    // Let's allocate a buffer on heap and never free it (leak is fine for test executable)
    // Or just use a static buffer if size is small.
    static std::vector<uint8_t> buffer(PPC_MEMORY_SIZE + 4096); // + padding
    // Align to page size if needed, but not critical for logic tests
    base = buffer.data();
}

void TestIsInMemoryRange() {
    std::cout << "Testing IsInMemoryRange..." << std::endl;
    // base is already set by constructor of g_memory

    uint8_t* start = g_memory.base;
    uint8_t* end = start + PPC_MEMORY_SIZE;

    // Inside range
    assert(g_memory.IsInMemoryRange(start));
    assert(g_memory.IsInMemoryRange(start + 100));
    assert(g_memory.IsInMemoryRange(end - 1));

    // Outside range
    assert(!g_memory.IsInMemoryRange(start - 1));
    assert(!g_memory.IsInMemoryRange(end));
    assert(!g_memory.IsInMemoryRange(nullptr)); // nullptr is usually 0, if base > 0

    std::cout << "IsInMemoryRange passed." << std::endl;
}

void TestTranslate() {
    std::cout << "Testing Translate..." << std::endl;

    uint8_t* start = g_memory.base;

    // Valid translations
    assert(g_memory.Translate(0) == start);
    assert(g_memory.Translate(100) == start + 100);
    assert(g_memory.Translate(PPC_MEMORY_SIZE - 1) == start + PPC_MEMORY_SIZE - 1);

    // Edge case: 0 offset
    assert(g_memory.Translate(0) == start);

    std::cout << "Translate passed." << std::endl;
}

void TestMapVirtual() {
    std::cout << "Testing MapVirtual..." << std::endl;

    uint8_t* start = g_memory.base;

    // Valid mappings
    assert(g_memory.MapVirtual(start) == 0);
    assert(g_memory.MapVirtual(start + 100) == 100);
    assert(g_memory.MapVirtual(start + PPC_MEMORY_SIZE - 1) == PPC_MEMORY_SIZE - 1);

    // Edge case: nullptr -> assert?
    // MapVirtual implementation: if (host) assert(IsInMemoryRange(host));
    // If host is nullptr, (uint8_t*)nullptr - base would be negative/large unsigned.
    // But wait, IsInMemoryRange(nullptr) checks if nullptr >= base.
    // If base is not nullptr, then nullptr < base (assuming flat address space starting at 0 and base > 0).
    // So IsInMemoryRange(nullptr) returns false.
    // Then assert fires.

    // So we should NOT pass nullptr to MapVirtual unless we expect assert.
    // We can't test assert failure easily without death tests.

    std::cout << "MapVirtual passed." << std::endl;
}

int main() {
    TestIsInMemoryRange();
    TestTranslate();
    TestMapVirtual();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
