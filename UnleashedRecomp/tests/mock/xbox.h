#pragma once
#include <cstdint>
#include <type_traits>
#include <bit>

template<typename T>
struct be
{
    T value;

    be() : value(0) {}
    be(T v) { set(v); }

    static T byteswap(T value)
    {
        if constexpr (sizeof(T) == 1) return value;
        else if constexpr (sizeof(T) == 2) return __builtin_bswap16((uint16_t)value);
        else if constexpr (sizeof(T) == 4) return __builtin_bswap32((uint32_t)value);
        else if constexpr (sizeof(T) == 8) return __builtin_bswap64((uint64_t)value);
        return value;
    }

    void set(T v) { value = byteswap(v); }
    T get() const { return byteswap(value); }

    operator T() const { return get(); }
    be& operator=(T v) { set(v); return *this; }
};
