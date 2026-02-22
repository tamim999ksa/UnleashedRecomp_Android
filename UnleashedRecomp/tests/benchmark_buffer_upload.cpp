#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>

#include "gpu/video_utils.h"

// Mock ByteSwap if not available via header in test environment (but it should be)
// In benchmark_iso_parsing.cpp they link XenonUtils, so byteswap.h should be available.

template<typename T>
void benchmark_copy_and_swap(const std::string& name, size_t num_elements, int iterations) {
    std::vector<T> src(num_elements);
    std::vector<T> dest(num_elements);

    // Initialize with random data
    std::mt19937 rng(42);
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    for (size_t i = 0; i < num_elements; ++i) {
        src[i] = static_cast<T>(dist(rng));
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        CopyAndSwap(dest.data(), src.data(), num_elements);
        // Prevent compiler optimization
        if (dest[0] == 0 && dest[num_elements-1] == 0) {
            std::cout << "";
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    double total_bytes = static_cast<double>(num_elements * sizeof(T) * iterations);
    double throughput = (total_bytes / (1024.0 * 1024.0)) / (elapsed.count() / 1000.0);

    std::cout << name << " Type: " << (sizeof(T) == 2 ? "uint16_t" : "uint32_t")
              << ", Elements: " << num_elements
              << ", Time: " << elapsed.count() << " ms"
              << ", Throughput: " << throughput << " MB/s" << std::endl;
}

int main() {
    std::cout << "Benchmarking CopyAndSwap..." << std::endl;

    // 1MB buffer
    size_t size_16 = 1024 * 512;
    size_t size_32 = 1024 * 256;
    int iterations = 1000;

    benchmark_copy_and_swap<uint16_t>("SIMD", size_16, iterations);
    benchmark_copy_and_swap<uint32_t>("SIMD", size_32, iterations);

    return 0;
}
