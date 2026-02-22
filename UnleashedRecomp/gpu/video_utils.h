#pragma once

#include "byteswap.h"
#include <cstddef>
#include <cstdint>

#if defined(__SSSE3__) || defined(__AVX__)
#include <tmmintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

template<typename T>
inline void CopyAndSwap(T* dest, const T* src, size_t count)
{
    size_t i = 0;

    // SIMD implementation for 16-bit integers
    if constexpr (sizeof(T) == 2)
    {
#if defined(__SSSE3__)
        const __m128i mask = _mm_setr_epi8(
            1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);

        for (; i + 8 <= count; i += 8) {
            __m128i val = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[i]));
            val = _mm_shuffle_epi8(val, mask);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dest[i]), val);
        }
#elif defined(__ARM_NEON)
        for (; i + 8 <= count; i += 8) {
            uint8x16_t val = vld1q_u8(reinterpret_cast<const uint8_t*>(&src[i]));
            val = vrev16q_u8(val);
            vst1q_u8(reinterpret_cast<uint8_t*>(&dest[i]), val);
        }
#endif
    }
    // SIMD implementation for 32-bit integers
    else if constexpr (sizeof(T) == 4)
    {
#if defined(__SSSE3__)
        const __m128i mask = _mm_setr_epi8(
            3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);

        for (; i + 4 <= count; i += 4) {
            __m128i val = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[i]));
            val = _mm_shuffle_epi8(val, mask);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&dest[i]), val);
        }
#elif defined(__ARM_NEON)
        for (; i + 4 <= count; i += 4) {
            uint8x16_t val = vld1q_u8(reinterpret_cast<const uint8_t*>(&src[i]));
            val = vrev32q_u8(val);
            vst1q_u8(reinterpret_cast<uint8_t*>(&dest[i]), val);
        }
#endif
    }

    // Scalar fallback
    for (; i < count; ++i)
    {
        dest[i] = ByteSwap(src[i]);
    }
}
