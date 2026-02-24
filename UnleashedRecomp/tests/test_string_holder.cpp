#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstdint>
#include <cstring>
#include <atomic>
#include <limits>
#include <cstdlib>
#include <iostream>

// --- Mocks ---

// Mock be (Big Endian) wrapper
template <typename T>
struct be {
    T value;
    be() = default;
    be(T v) : value(v) {}
    operator T() const { return value; }
    be& operator=(T v) { value = v; return *this; }
};

// Mock __HH_ALLOC and __HH_FREE
void* g_lastAllocPtr = nullptr;
uint32_t g_lastAllocSize = 0;
bool g_allocCalled = false;

void* __HH_ALLOC(const uint32_t in_Size) {
    g_allocCalled = true;
    g_lastAllocSize = in_Size;
    if (in_Size == 0) return nullptr;
    void* ptr = malloc(in_Size);
    g_lastAllocPtr = ptr;
    return ptr;
}

void __HH_FREE(const void* in_pData) {
    if (in_pData) free((void*)in_pData);
}

// Mock strlen and strcpy
size_t g_mockStrlenResult = 0;
bool g_useMockStrlen = false;

size_t my_strlen(const char* s) {
    if (g_useMockStrlen) return g_mockStrlenResult;
    return std::strlen(s);
}

char* my_strcpy(char* dest, const char* src) {
    // If mocking huge length, don't actually copy potentially huge data
    // Just null-terminate to be safe
    if (g_useMockStrlen) {
        if (dest) *dest = '\0';
        return dest;
    }
    return std::strcpy(dest, src);
}

// Intercept standard functions BEFORE including the implementation
#define strlen my_strlen
#define strcpy my_strcpy

namespace Hedgehog::Base
{
    struct SStringHolder
    {
        be<uint32_t> RefCount;
        char aStr[];

        static SStringHolder* GetHolder(const char* in_pStr);
        static SStringHolder* Make(const char* in_pStr);

        void AddRef();
        void Release();

        bool IsUnique() const;
    };
}

// Include the implementation file directly
#include "../api/Hedgehog/Base/Type/detail/hhStringHolder.inl"

// Reset macro definitions
#undef strlen
#undef strcpy

using namespace Hedgehog::Base;

TEST_CASE("SStringHolder::Make behavior") {
    // Reset state before each test case run? Doctest runs test cases independently? No, state persists if global.
    // So reset at start of test case.
    g_allocCalled = false;
    g_lastAllocSize = 0;
    g_lastAllocPtr = nullptr;
    g_useMockStrlen = false;
    g_mockStrlenResult = 0;

    SUBCASE("Normal allocation works") {
        const char* str = "Hello";
        SStringHolder* holder = SStringHolder::Make(str);

        CHECK(holder != nullptr);
        CHECK(g_allocCalled == true);
        // Size: 4 (RefCount) + 5 (len) + 1 (null) = 10
        CHECK(g_lastAllocSize == 10);

        if (holder) {
            CHECK(holder->RefCount == 1);
            CHECK(std::strcmp(holder->aStr, str) == 0);
            holder->Release();
        }
    }

    SUBCASE("Integer overflow vulnerability check") {
        // Set up huge string length simulation
        g_useMockStrlen = true;
        // Target: overflow uint32_t
        // sizeof(RefCount) = 4
        // Formula: 4 + len + 1
        // len = UINT32_MAX - 3 -> 4 + (UINT32_MAX - 3) + 1 = UINT32_MAX + 2 = 1 (overflow)

        g_mockStrlenResult = (size_t)UINT32_MAX - 3;

        SStringHolder* holder = SStringHolder::Make("dummy");

        // Vulnerable behavior: returns non-null pointer, allocated size is small (1 byte)
        // Fixed behavior: returns nullptr, allocation not called or handles overflow

        // This test EXPECTS the FIX. So if vulnerable, it should FAIL.
        CHECK(holder == nullptr);

        // If allocation happened, it must not be the overflowed small size
        if (g_allocCalled) {
             CHECK(g_lastAllocSize > 100); // Expect large allocation attempt if any (which would fail ideally or be caught)
             // But actually we expect Make to return nullptr BEFORE allocation or AFTER allocation failure
        }

        if (holder) holder->Release();
    }
}
