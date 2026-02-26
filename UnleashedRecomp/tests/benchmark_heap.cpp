#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <sys/mman.h>

// Mock dependencies
extern "C" {
#include "../../thirdparty/o1heap/o1heap.c"
}

// Simple Mock Memory
struct Memory {
    uint8_t* base;
    size_t capacity;
    Memory() {
        capacity = 0x20000000ULL; // 512MB
        // Use mmap for page alignment (4KB) which satisfies O1HEAP (32 bytes)
        base = (uint8_t*)mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(base == MAP_FAILED) {
            std::cerr << "Failed to mmap memory" << std::endl;
            exit(1);
        }
    }
    ~Memory() { munmap(base, capacity); }
    void* Translate(size_t offset) {
        if (offset < 0x20000) return base;
        return base + (offset - 0x20000);
    }
} g_memory;

// Constants
const size_t HEAP_OFFSET = 0x20000;
const size_t HEAP_SIZE = 0x10000000; // 256MB Heap

// 1. Global Lock Heap (Baseline Implementation)
class GlobalLockHeap {
    std::mutex mutex;
    O1HeapInstance* heap;
public:
    void Init() {
        void* ptr = g_memory.Translate(HEAP_OFFSET);
        heap = o1heapInit(ptr, HEAP_SIZE);
        if(!heap) {
            std::cerr << "GlobalLockHeap Init failed" << std::endl;
            exit(1);
        }
    }
    void* Alloc(size_t size) {
        std::lock_guard<std::mutex> lock(mutex);
        return o1heapAllocate(heap, size > 0 ? size : 1);
    }
    void Free(void* ptr) {
        if(!ptr) return;
        std::lock_guard<std::mutex> lock(mutex);
        o1heapFree(heap, ptr);
    }
};

// 2. Sharded Heap (Optimized Implementation)
class ShardedHeap {
    static const int NUM_SHARDS = 32;
    struct Shard {
        std::mutex mutex;
        O1HeapInstance* heap;
    };
    Shard shards[NUM_SHARDS];
    size_t shard_size;
    void* heap_base;

public:
    void Init() {
        shard_size = HEAP_SIZE / NUM_SHARDS;
        heap_base = g_memory.Translate(HEAP_OFFSET);

        for(int i=0; i<NUM_SHARDS; ++i) {
            size_t offset = HEAP_OFFSET + i * shard_size;
            void* ptr = g_memory.Translate(offset);
            shards[i].heap = o1heapInit(ptr, shard_size);
             if(!shards[i].heap) {
                std::cerr << "ShardedHeap Init failed at shard " << i << std::endl;
                exit(1);
            }
        }
    }

    void* Alloc(size_t size) {
        // Fast fail for large allocs
        if (size > shard_size) return nullptr;

        // Thread-local hint
        static std::atomic<uint32_t> threadCounter{0};
        static thread_local int hint = -1;
        if (hint == -1) {
            hint = threadCounter.fetch_add(1) % NUM_SHARDS;
        }

        int idx = hint;
        // Try hinted shard
        {
            std::lock_guard<std::mutex> lock(shards[idx].mutex);
            void* ptr = o1heapAllocate(shards[idx].heap, size > 0 ? size : 1);
            if (ptr) return ptr;
        }

        // Try others
        for(int i=1; i<NUM_SHARDS; ++i) {
            int try_idx = (idx + i) % NUM_SHARDS;
            std::lock_guard<std::mutex> lock(shards[try_idx].mutex);
            void* ptr = o1heapAllocate(shards[try_idx].heap, size > 0 ? size : 1);
            if (ptr) {
                return ptr;
            }
        }
        return nullptr;
    }

    void Free(void* ptr) {
        if(!ptr) return;

        // Fast index calculation using cached base
        uintptr_t offset_in_heap = (uintptr_t)ptr - (uintptr_t)heap_base;

        if (offset_in_heap >= HEAP_SIZE) return;

        int idx = offset_in_heap / shard_size;
        if(idx >= 0 && idx < NUM_SHARDS) {
            std::lock_guard<std::mutex> lock(shards[idx].mutex);
            o1heapFree(shards[idx].heap, ptr);
        }
    }
};

template<typename HeapType>
void run_benchmark(HeapType& heap, const char* name) {
    heap.Init();
    int num_threads = 8;
    int ops_per_thread = 200000;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for(int i=0; i<num_threads; ++i) {
        threads.emplace_back([&heap, ops_per_thread, i]() {
             std::vector<void*> ptrs;
             ptrs.reserve(ops_per_thread);
             std::mt19937 rng(i);
             std::uniform_int_distribution<size_t> dist(16, 512);

             for(int j=0; j<ops_per_thread; ++j) {
                 if(ptrs.empty() || (rng() % 100 < 60)) {
                     void* p = heap.Alloc(dist(rng));
                     if(p) ptrs.push_back(p);
                 } else {
                     size_t idx = rng() % ptrs.size();
                     heap.Free(ptrs[idx]);
                     ptrs[idx] = ptrs.back();
                     ptrs.pop_back();
                 }
             }
             for(void* p : ptrs) heap.Free(p);
        });
    }

    for(auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << name << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
}

int main() {
    {
        GlobalLockHeap global;
        run_benchmark(global, "Global Lock");
    }
    {
        ShardedHeap sharded;
        run_benchmark(sharded, "Sharded");
    }
    return 0;
}
