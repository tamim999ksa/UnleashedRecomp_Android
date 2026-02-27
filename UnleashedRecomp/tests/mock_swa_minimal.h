#pragma once
#include <cstdint>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#define SWA_CONCAT2(x, y) x##y
#define SWA_CONCAT(x, y) SWA_CONCAT2(x, y)
#define SWA_INSERT_PADDING(length) uint8_t SWA_CONCAT(pad, __LINE__)[length]
#define SWA_ASSERT_OFFSETOF(type, field, offset) static_assert(offsetof(type, field) == offset)
#define SWA_ASSERT_SIZEOF(type, size) static_assert(sizeof(type) == size)

struct swa_null_ctor {};

template <typename T>
struct xpointer {
    uint32_t ptr;
    xpointer() : ptr(0) {}
    xpointer(const swa_null_ctor&) : ptr(0) {}
};

template <typename T>
struct be {
    T val;
    be() : val(0) {}
    be(T v) : val(v) {}
    operator T() const { return val; }
};

namespace Hedgehog::Base {
    class CSharedString {
        void* pData;
    };
    class CObject {
    public:
        CObject() {}
        CObject(const swa_null_ctor&) {}
        virtual ~CObject() = default;
    };
}
