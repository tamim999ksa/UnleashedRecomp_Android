#define TOOLS_XENONRECOMP_XENONUTILS_XBOX_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

template<typename T>
struct be {
    T val;
    operator T() const { return val; }
    be(T v) : val(v) {}
    be() = default;
};

struct PPCContext {
    union { uint64_t u64; double f64; } f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
    union { uint64_t u64; uint32_t u32; } r3, r4, r5, r6, r7, r8, r9, r10, r1, r13;
    uint32_t fpscr;
};

struct Memory {
    uint8_t* base;
    template<typename T = void>
    T* Translate(uint32_t addr) { return reinterpret_cast<T*>(base + addr); }
};
extern Memory g_memory;

inline void* MmGetHostAddress(uint32_t addr) { return g_memory.Translate<void>(addr); }

template<typename T>
struct xpointer {
    uint32_t addr;
    operator T*() const { return g_memory.Translate<T>(addr); }
    xpointer(std::nullptr_t) : addr(0) {}
    xpointer() = default;
};

inline PPCContext* GetPPCContext() { static PPCContext ctx; return &ctx; }
inline void SetPPCContext(const PPCContext&) {}

// Include the override header directly
#include "build_overrides/UnleashedRecomp/api/SWA/System/InputState.h"

int main() {}
