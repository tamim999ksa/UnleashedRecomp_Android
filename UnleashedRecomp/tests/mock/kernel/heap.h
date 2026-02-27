#pragma once
#include <cstddef>
#include <utility>
#include <cstdint>

struct O1HeapInstance {};

struct Heap
{
    static constexpr int NUM_SHARDS = 32;
    void* heaps[NUM_SHARDS];
    size_t lastAllocSize = 0;

    void* Alloc(size_t size)
    {
        lastAllocSize = size;
        if (size > 1024 * 1024 * 512)
            return nullptr;
        return new uint8_t[size];
    }

    void Free(void* ptr)
    {
        if (ptr) delete[] (uint8_t*)ptr;
    }

    template<typename T, typename... Args>
    T* Alloc(Args&&... args)
    {
        T* obj = (T*)Alloc(sizeof(T));
        if(obj) new (obj) T(std::forward<Args>(args)...);
        return obj;
    }
};

extern Heap g_userHeap;
