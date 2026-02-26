#ifndef UNIT_TEST
#include <stdafx.h>
#else
#include <algorithm>
#include <mutex>
#include <cstring>
#include <cassert>
#include <new>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <o1heap.h>
#include <atomic>
#endif

#include "heap.h"
#include "memory.h"
#ifndef UNIT_TEST
#include "function.h"
#include <atomic>
#endif

constexpr size_t RESERVED_BEGIN = 0x7FEA0000;
constexpr size_t RESERVED_END = 0xA0000000;

void Heap::Init()
{
    size_t totalSize = RESERVED_BEGIN - 0x20000;
    shardSize = totalSize / NUM_SHARDS;
    heapBase = g_memory.Translate(0x20000);

    for (int i = 0; i < NUM_SHARDS; i++)
    {
        heaps[i] = o1heapInit(g_memory.Translate(0x20000 + i * shardSize), shardSize);
    }

    physicalHeap = o1heapInit(g_memory.Translate(RESERVED_END), 0x100000000 - RESERVED_END);
}

void* Heap::Alloc(size_t size)
{
    static std::atomic<uint32_t> threadCounter{0};
    static thread_local int hint = -1;

    if (hint == -1)
        hint = threadCounter.fetch_add(1) % NUM_SHARDS;

    int idx = hint;

    // Try hinted shard
    {
        std::lock_guard lock(mutexes[idx]);
        void* ptr = o1heapAllocate(heaps[idx], std::max<size_t>(1, size));
        if (ptr) return ptr;
    }

    // Try others
    for (int i = 1; i < NUM_SHARDS; i++)
    {
        int tryIdx = (idx + i) % NUM_SHARDS;
        std::lock_guard lock(mutexes[tryIdx]);
        void* ptr = o1heapAllocate(heaps[tryIdx], std::max<size_t>(1, size));
        if (ptr) return ptr;
    }

    return nullptr;
}

void* Heap::AllocPhysical(size_t size, size_t alignment)
{
    size = std::max<size_t>(1, size);
    alignment = alignment == 0 ? 0x1000 : std::max<size_t>(16, alignment);

    std::lock_guard lock(physicalMutex);

    void* ptr = o1heapAllocate(physicalHeap, size + alignment);
    size_t aligned = ((size_t)ptr + alignment) & ~(alignment - 1);

    *((void**)aligned - 1) = ptr;
    *((size_t*)aligned - 2) = size + O1HEAP_ALIGNMENT;

    return (void*)aligned;
}

void Heap::Free(void* ptr)
{
    if (ptr >= physicalHeap)
    {
        std::lock_guard lock(physicalMutex);
        o1heapFree(physicalHeap, *((void**)ptr - 1));
    }
    else
    {
        uintptr_t offset = (uintptr_t)ptr - (uintptr_t)heapBase;
        int idx = (int)(offset / shardSize);

        if (idx >= 0 && idx < NUM_SHARDS)
        {
            std::lock_guard lock(mutexes[idx]);
            o1heapFree(heaps[idx], ptr);
        }
    }
}

size_t Heap::Size(void* ptr)
{
    if (ptr)
        return *((size_t*)ptr - 2) - O1HEAP_ALIGNMENT; // relies on fragment header in o1heap.c

    return 0;
}

#ifndef UNIT_TEST
uint32_t RtlAllocateHeap(uint32_t heapHandle, uint32_t flags, uint32_t size)
{
    void* ptr = g_userHeap.Alloc(size);
    if ((flags & 0x8) != 0)
        memset(ptr, 0, size);

    assert(ptr);
    return g_memory.MapVirtual(ptr);
}

uint32_t RtlReAllocateHeap(uint32_t heapHandle, uint32_t flags, uint32_t memoryPointer, uint32_t size)
{
    void* ptr = g_userHeap.Alloc(size);

    size_t oldSize = 0;
    if (memoryPointer != 0)
    {
        void* oldPtr = g_memory.Translate(memoryPointer);
        oldSize = g_userHeap.Size(oldPtr);
        memcpy(ptr, oldPtr, std::min<size_t>(size, oldSize));
        g_userHeap.Free(oldPtr);
    }

    if ((flags & 0x8) != 0 && size > oldSize)
        memset((char*)ptr + oldSize, 0, size - oldSize);

    assert(ptr);
    return g_memory.MapVirtual(ptr);
}

uint32_t RtlFreeHeap(uint32_t heapHandle, uint32_t flags, uint32_t memoryPointer)
{
    if (memoryPointer != NULL)
        g_userHeap.Free(g_memory.Translate(memoryPointer));

    return true;
}

uint32_t RtlSizeHeap(uint32_t heapHandle, uint32_t flags, uint32_t memoryPointer)
{
    if (memoryPointer != NULL)
        return (uint32_t)g_userHeap.Size(g_memory.Translate(memoryPointer));

    return 0;
}

uint32_t XAllocMem(uint32_t size, uint32_t flags)
{
    void* ptr = (flags & 0x80000000) != 0 ?
        g_userHeap.AllocPhysical(size, (1ull << ((flags >> 24) & 0xF))) :
        g_userHeap.Alloc(size);

    if ((flags & 0x40000000) != 0)
        memset(ptr, 0, size);

    assert(ptr);
    return g_memory.MapVirtual(ptr);
}

void XFreeMem(uint32_t baseAddress, uint32_t flags)
{
    if (baseAddress != NULL)
        g_userHeap.Free(g_memory.Translate(baseAddress));
}

GUEST_FUNCTION_STUB(sub_82BD7788); // HeapCreate
GUEST_FUNCTION_STUB(sub_82BD9250); // HeapDestroy

GUEST_FUNCTION_HOOK(sub_82BD7D30, RtlAllocateHeap);
GUEST_FUNCTION_HOOK(sub_82BD8600, RtlFreeHeap);
GUEST_FUNCTION_HOOK(sub_82BD88F0, RtlReAllocateHeap);
GUEST_FUNCTION_HOOK(sub_82BD6FD0, RtlSizeHeap);

GUEST_FUNCTION_HOOK(sub_831CC9C8, XAllocMem);
GUEST_FUNCTION_HOOK(sub_831CCA60, XFreeMem);
#endif
