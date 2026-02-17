#include <iostream>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <memory>
#include <new>
#include <algorithm>
#include <vector>

// Define necessary macros for XenonUtils/ppc_context.h and Memory
#ifndef PPC_MEMORY_SIZE
#define PPC_MEMORY_SIZE 0x100000000ull
#endif

#define PPC_IMAGE_BASE 0x82000000
#define PPC_IMAGE_SIZE 0x1000000
#define PPC_CODE_BASE 0x82000000
#define PPC_CONFIG_H_INCLUDED

// We need to include the full definition of PPCContext from tools
#include "../../tools/XenonRecomp/XenonUtils/ppc_context.h"

// Define PPC_LOOKUP_FUNC required by Memory definition if not defined by ppc_context.h
#ifndef PPC_LOOKUP_FUNC
#define PPC_LOOKUP_FUNC(x, y) *(PPCFunc**)(x + PPC_IMAGE_BASE + PPC_IMAGE_SIZE + (uint64_t(uint32_t(y) - PPC_CODE_BASE) * 2))
#endif

// Include Memory definition
// We assume -I. (root) and -IUnleashedRecomp
#include "UnleashedRecomp/kernel/memory.h"

// Define g_memory
Memory g_memory;

// Implement Memory constructor/methods if needed
// Memory.h has "Memory();" declaration.
// We implement it here.
Memory::Memory() {
    // Allocate 1GB for testing (or less if possible, but translate checks assertions)
    // PPC_MEMORY_SIZE is 4GB (0x100000000ull).
    // We can't easily allocate 4GB contiguous on all systems.
    // But Translate uses base + offset.
    // For the test, we only need valid memory where we access.
    // The guest stack usually starts high.
    // Let's allocate a smaller buffer and mock Translate to point to it if address is in stack range?
    // guest_stack_var sets m_ptr = r1 - size.
    // If r1 starts at some value, we need memory there.

    // Let's allocate 1MB for stack and set r1 to point to the end of it.
    // But Memory class assumes `base` covers the whole 4GB address space logic?
    // "IsInMemoryRange(host) return host >= base && host < base + PPC_MEMORY_SIZE"
    // "Translate(offset) return base + offset"

    // If we allocate a small buffer, `base` + large_offset might crash if accessed.
    // But `guest_stack_var` accesses memory via `Translate(m_ptr)`.
    // If we set `base` to a pointer such that `base + stack_ptr` is valid memory, we are good.
    // Say we allocate 1MB `real_stack`.
    // We want `base + stack_ptr` = `real_stack + offset_in_stack`.
    // Let `stack_ptr` be `0x70000000` (just an example).
    // We want `base + 0x70000000` to be valid.
    // This requires `base` to be `valid_ptr - 0x70000000`.
    // This `base` might be an invalid pointer itself, but as long as we don't dereference it directly...
    // The C++ standard says pointer arithmetic is only valid within array bounds.
    // But typical compilers handle `ptr + offset` as integer arithmetic.

    // For safety, let's just allocate a large enough buffer if possible, or map it.
    // Or simpler: define a stack pointer that is small enough?
    // If `r1` is 0x10000, and we allocate 0x20000 bytes.
    // `base` = `allocated_buffer`.
    // Then `Translate(0x10000)` = `buffer + 0x10000`.

    // So we will set `r1` to a low value like 0x10000 (64KB).
    // And allocate 1MB buffer.

    size_t alloc_size = 1024 * 1024; // 1MB
    base = new uint8_t[alloc_size];
    memset(base, 0, alloc_size);
}

// We need a way to clean up memory in test, but g_memory is global.
// We can rely on OS cleanup or add a destructor helper.

// Include guest_stack_var
#include "UnleashedRecomp/cpu/guest_stack_var.h"

// Needed for MmGetHostAddress if used (it is extern "C" in memory.h)
extern "C" void* MmGetHostAddress(uint32_t ptr) {
    return g_memory.Translate(ptr);
}

// Test Helper
void SetupContext(uint32_t stack_ptr) {
    PPCContext* ctx = GetPPCContext();
    // We need to initialize g_ppcContext.
    // UnleashedRecomp/cpu/ppc_context.h defines it as thread_local pointer.
    // But it's null by default.
    // We need a PPCContext instance.
    static PPCContext static_ctx;
    SetPPCContext(static_ctx);

    GetPPCContext()->r1.u32 = stack_ptr;
}

void TestBasicAllocation() {
    std::cout << "Testing Basic Allocation..." << std::endl;
    SetupContext(0x10000); // 64KB stack top

    uint32_t original_sp = GetPPCContext()->r1.u32;

    {
        guest_stack_var<int> var(42);

        uint32_t new_sp = GetPPCContext()->r1.u32;
        assert(new_sp < original_sp);
        assert(new_sp % 8 == 0); // Alignment
        assert(*var == 42);

        // Verify value in memory
        int* host_ptr = (int*)g_memory.Translate(new_sp);
        assert(*host_ptr == 42);
    }

    uint32_t restored_sp = GetPPCContext()->r1.u32;
    assert(restored_sp == original_sp);

    std::cout << "Passed." << std::endl;
}

void TestAssignment() {
    std::cout << "Testing Assignment..." << std::endl;
    SetupContext(0x10000);

    {
        guest_stack_var<int> var1(10);
        guest_stack_var<int> var2(20);

        var2 = var1; // Copy assignment
        assert(*var2 == 10);
        assert(var1.get() != var2.get());

        guest_stack_var<int> var3(30);
        var3 = std::move(var1); // Move assignment
        assert(*var3 == 10);
        assert(var3.get() != var1.get());

        var3 = 40; // Assign T const&
        assert(*var3 == 40);

        var3 = 50; // Assign T&&
        assert(*var3 == 50);
    }
    std::cout << "Passed." << std::endl;
}

void TestAlignment() {
    std::cout << "Testing Alignment..." << std::endl;
    SetupContext(0x10000);

    struct alignas(16) AlignedStruct {
        int x;
    };

    {
        guest_stack_var<AlignedStruct> var;
        uint32_t sp = GetPPCContext()->r1.u32;
        assert(sp % 16 == 0);
        assert(sp % 8 == 0); // Should also be 8 aligned (guest_stack_var enforces max(alignof, 8))
    }

    struct SmallStruct {
        char c;
    };

    {
        guest_stack_var<SmallStruct> var;
        uint32_t sp = GetPPCContext()->r1.u32;
        assert(sp % 8 == 0); // Enforced min alignment 8
    }

    std::cout << "Passed." << std::endl;
}

void TestNestedVariables() {
    std::cout << "Testing Nested Variables..." << std::endl;
    SetupContext(0x10000);
    uint32_t sp0 = GetPPCContext()->r1.u32;

    {
        guest_stack_var<int> var1(1);
        uint32_t sp1 = GetPPCContext()->r1.u32;
        assert(sp1 < sp0);

        {
            guest_stack_var<double> var2(3.14);
            uint32_t sp2 = GetPPCContext()->r1.u32;
            assert(sp2 < sp1);
            assert(*var2 == 3.14);
        }

        assert(GetPPCContext()->r1.u32 == sp1);
    }

    assert(GetPPCContext()->r1.u32 == sp0);
    std::cout << "Passed." << std::endl;
}

void TestCopyAndMove() {
    std::cout << "Testing Copy and Move..." << std::endl;
    SetupContext(0x10000);

    {
        guest_stack_var<int> var1(123);
        guest_stack_var<int> var2(var1); // Copy ctor

        assert(*var2 == 123);
        assert(var1.get() != var2.get());

        guest_stack_var<int> var3(std::move(var1)); // Move ctor
        assert(*var3 == 123);
        assert(var3.get() != var1.get()); // Different stack locations
    }
    std::cout << "Passed." << std::endl;
}

int main() {
    try {
        TestBasicAllocation();
        TestAlignment();
        TestNestedVariables();
        TestCopyAndMove();
        TestAssignment();

        std::cout << "All tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
