#pragma once

#include "mutex.h"

struct Heap
{
    static constexpr int NUM_SHARDS = 32;

    Mutex mutexes[NUM_SHARDS];
    O1HeapInstance* heaps[NUM_SHARDS];
    size_t shardSize;
    void* heapBase;

    Mutex physicalMutex;
    O1HeapInstance* physicalHeap;

    void Init();

    void* Alloc(size_t size);
    void* AllocPhysical(size_t size, size_t alignment);
    void Free(void* ptr);

    size_t Size(void* ptr);

    template<typename T, typename... Args>
    T* Alloc(Args&&... args)
    {
        T* obj = (T*)Alloc(sizeof(T));
        new (obj) T(std::forward<Args>(args)...);
        return obj;
    }

    template<typename T, typename... Args>
    T* AllocPhysical(Args&&... args)
    {
        T* obj = (T*)AllocPhysical(sizeof(T), alignof(T));
        new (obj) T(std::forward<Args>(args)...);
        return obj;
    }
};

extern Heap g_userHeap;
