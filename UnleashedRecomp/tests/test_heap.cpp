#include <mutex>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#define UNIT_TEST

#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>

using PPCFunc = void(void*);
#define PPC_LOOKUP_FUNC(base, guest) (*(PPCFunc**)((uint8_t*)base + guest))
// Define PPC_MEMORY_SIZE large enough for physical heap (4GB range)
#define PPC_MEMORY_SIZE 0x100000001ULL

// Forward declare MmGetHostAddress
extern "C" void* MmGetHostAddress(uint32_t ptr);

#include "../kernel/memory.h"

// Implement Memory constructor for test environment
Memory::Memory() {
    // Allocate 4GB + 4KB to cover the entire simulated Xbox 360 address space
    // We use mmap to reserve virtual address space without committing all physical memory immediately
    size_t size = (size_t)PPC_MEMORY_SIZE + 4096;

    // Use MAP_NORESERVE if available to be nicer to the system, but for 4GB on 64-bit it's usually fine
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to allocate 4GB memory for Heap test. Ensure you are running on a 64-bit system." << std::endl;
        std::exit(1);
    }

    base = (uint8_t*)ptr;

    // Zero out the memory (mmap usually does this, but good to be sure if we reuse)
    // std::memset(base, 0, size); // Avoid touching all pages to keep RSS low
}

// Global memory instance
Memory g_memory;

extern "C" void* MmGetHostAddress(uint32_t ptr) {
    return g_memory.Translate(ptr);
}

// Include the implementation to test
#include <o1heap.h>
#include "../kernel/heap.h"

// Define global heap instance for testing
Heap g_userHeap;

#include "../kernel/heap.cpp"

TEST_CASE("Heap Initialization") {
    // Verify initialization works
    CHECK_NOTHROW(g_userHeap.Init());

    CHECK(g_userHeap.heaps[0] != nullptr);
    CHECK(g_userHeap.physicalHeap != nullptr);

    // Verify physical heap pointer is at the expected location (start of reserved end)
    // o1heapInit places the instance at the start of the buffer
    CHECK((void*)g_userHeap.physicalHeap == g_memory.Translate(RESERVED_END));
}

TEST_CASE("Heap Allocation") {
    // Ensure initialized
    if (!g_userHeap.heaps[0]) g_userHeap.Init();

    SUBCASE("Basic Allocation") {
        void* ptr = g_userHeap.Alloc(128);
        CHECK(ptr != nullptr);

        // Write to memory to ensure it's valid
        std::memset(ptr, 0x55, 128);
        CHECK(*(uint8_t*)ptr == 0x55);

        size_t size = g_userHeap.Size(ptr);
        CHECK(size >= 128);

        g_userHeap.Free(ptr);
    }

    SUBCASE("Zero Size Allocation") {
        void* ptr = g_userHeap.Alloc(0);
        CHECK(ptr != nullptr);
        g_userHeap.Free(ptr);
    }

    SUBCASE("Multiple Allocations") {
        void* ptr1 = g_userHeap.Alloc(64);
        void* ptr2 = g_userHeap.Alloc(64);

        CHECK(ptr1 != nullptr);
        CHECK(ptr2 != nullptr);
        CHECK(ptr1 != ptr2);

        g_userHeap.Free(ptr1);
        g_userHeap.Free(ptr2);
    }
}

TEST_CASE("Physical Heap Allocation") {
    if (!g_userHeap.physicalHeap) g_userHeap.Init();

    SUBCASE("Aligned Allocation") {
        size_t size = 1024;
        size_t alignment = 4096;

        void* ptr = g_userHeap.AllocPhysical(size, alignment);
        CHECK(ptr != nullptr);

        // Check alignment
        CHECK(((uintptr_t)ptr % alignment) == 0);

        // Check address range (should be in physical heap region)
        uintptr_t ptrAddr = (uintptr_t)ptr;
        uintptr_t baseAddr = (uintptr_t)g_memory.base;
        CHECK(ptrAddr >= baseAddr + RESERVED_END);

        // Check size
        // AllocPhysical stores header before the pointer, Size() relies on that?
        // Heap::Size implementation: *((size_t*)ptr - 2) - O1HEAP_ALIGNMENT;
        // Heap::AllocPhysical implementation:
        // *((size_t*)aligned - 2) = size + O1HEAP_ALIGNMENT;
        // So Size(ptr) should return size.

        size_t reportedSize = g_userHeap.Size(ptr);
        CHECK(reportedSize == size); // Should be exact match due to how it's stored

        g_userHeap.Free(ptr);
    }

    SUBCASE("Default Alignment") {
        void* ptr = g_userHeap.AllocPhysical(512, 0);
        CHECK(ptr != nullptr);
        CHECK(((uintptr_t)ptr % 0x1000) == 0); // Default 0x1000
        g_userHeap.Free(ptr);
    }
}

TEST_CASE("Heap Reuse") {
    void* ptr = g_userHeap.Alloc(256);
    CHECK(ptr != nullptr);
    g_userHeap.Free(ptr);

    // Allocate again, might get same pointer or different
    void* ptr2 = g_userHeap.Alloc(256);
    CHECK(ptr2 != nullptr);
    g_userHeap.Free(ptr2);
}

TEST_CASE("Templated Alloc") {
    struct TestStruct {
        int x;
        float y;
        TestStruct(int a, float b) : x(a), y(b) {}
    };

    TestStruct* obj = g_userHeap.Alloc<TestStruct>(42, 3.14f);
    CHECK(obj != nullptr);
    CHECK(obj->x == 42);
    CHECK(obj->y == 3.14f);

    g_userHeap.Free(obj);
}

TEST_CASE("Templated AllocPhysical") {
    struct alignas(128) AlignedStruct {
        char data[128];
        AlignedStruct() { data[0] = 'A'; }
    };

    AlignedStruct* obj = g_userHeap.AllocPhysical<AlignedStruct>();
    CHECK(obj != nullptr);
    CHECK(((uintptr_t)obj % 128) == 0);
    CHECK(obj->data[0] == 'A');

    g_userHeap.Free(obj);
}
