#pragma once
#include <cstdint>

struct PPCContext
{
    union { uint64_t u64; uint32_t u32; } r1, r3, r4, r5, r6, r7, r8, r9, r10, r13;
    union { double f64; } f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
    uint64_t fpscr;
};
