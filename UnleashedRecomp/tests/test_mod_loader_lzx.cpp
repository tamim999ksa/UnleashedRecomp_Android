#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <map>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <ankerl/unordered_dense.h>
#include <string>
#include <string_view>
#include <fmt/format.h>
#include <xxhash.h>
#include <span>
#include <charconv>
#include <cassert>

#define UNIT_TEST
#define PPC_MEMORY_SIZE 0x20000000

// Mock Macros
#define PPC_FUNC_IMPL(name) void name##_impl(PPCContext& ctx, uint8_t* base)
#define PPC_FUNC(name) void name(PPCContext& ctx, uint8_t* base)
#define GUEST_FUNCTION_HOOK(name, impl)
#define GUEST_FUNCTION_STUB(name)
#define LOGF_IMPL(...)
#undef PPC_STORE_U32
#define PPC_STORE_U32(x, v) (void)(x)
#undef PPC_STORE_U8
#define PPC_STORE_U8(x, v) (void)(x)
#define PPC_LOAD_U32(x) (0)

// Mock SDL
using SDL_Scancode = int;
#define SDL_SCANCODE_UNKNOWN 0
#define SDL_SCANCODE_RIGHT 0

// Struct PPCContext definition
struct PPCContext
{
    union { uint32_t u32; uint64_t u64; } r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16, r17, r18, r19, r20, r21, r22, r23, r24, r25, r26, r27, r28, r29, r30, r31;
    union { double f64; } f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
    uint32_t fpscr;
};

// Mock Heap
#include "kernel/heap.h"

// Real Memory header logic
#define PPC_LOOKUP_FUNC(base, guest) (*(PPCFunc**)(0))
using PPCFunc = void(PPCContext&, uint8_t*);
#include "kernel/memory.h"

// Implement Memory constructor/destructor/helpers for real header
Memory::Memory() { base = (uint8_t*)calloc(1, PPC_MEMORY_SIZE); }
extern "C" void* MmGetHostAddress(uint32_t ptr) { return g_memory.Translate(ptr); }

// Global Instances
Heap g_userHeap;
Memory g_memory;

// Mock Config
#include "user/config.h"
std::vector<ConfigDefinition*> g_configDefinitions;

// Mock Guest Functions
void sub_831CE1A0(PPCContext& ctx, uint8_t* base) {}
void sub_831CE0D0(PPCContext& ctx, uint8_t* base) {}
void sub_831CE150(PPCContext& ctx, uint8_t* base) {}

void sub_822C0988(PPCContext&, uint8_t*) {}
void sub_822C0270(PPCContext&, uint8_t*) {}
void sub_82DFB148(PPCContext&, uint8_t*) {}
void sub_83167FD8(PPCContext&, uint8_t*) {}
void sub_83168100(PPCContext&, uint8_t*) {}

void __imp__sub_82E0D3E8(PPCContext&, uint8_t*) {}
void __imp__sub_82E0CC38(PPCContext&, uint8_t*) {}
void __imp__sub_82E140D8(PPCContext&, uint8_t*) {}
void __imp__sub_82E0B500(PPCContext&, uint8_t*) {}
void __imp__sub_8314A310(PPCContext&, uint8_t*) {}

// Mock Headers
namespace os::process { inline void ShowConsole() {} }
#include "user/paths.h"

#include "../mod/mod_loader.cpp"

TEST_CASE("decompressLzx security checks")
{
    PPCContext ctx{};
    uint8_t* base = g_memory.base;
    uint8_t* buffer = base + 0x1000;

    SUBCASE("Reject header with huge UncompressedSize")
    {
        LzxHeader* header = reinterpret_cast<LzxHeader*>(buffer);

        header->Signature = LZX_SIGNATURE;
        header->UncompressedSize = 0x80000000;
        header->WindowSize = 0x20000;
        header->BlockSizeParam = 0x0;

        g_userHeap.lastAllocSize = 0;

        auto result = decompressLzx(ctx, base, buffer, sizeof(LzxHeader), nullptr);

        CHECK(result.data() == nullptr);
        CHECK(g_userHeap.lastAllocSize != 0x80000000);
    }

    SUBCASE("Reject buffer smaller than header")
    {
        auto result = decompressLzx(ctx, base, buffer, sizeof(LzxHeader) - 1, nullptr);
        CHECK(result.empty());
    }
}
