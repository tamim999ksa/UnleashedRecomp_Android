#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <random>

#include "install/iso_file_system.h"

// Mock ISOFileSystem to allow manual population of fileMap without valid ISO
// We just use the struct as is, but since we can't easily open a valid ISO,
// we rely on the fact that fileMap is public and we can populate it.
// However, the constructor tries to open a file. We can pass a dummy file.

void run_benchmark() {
    // Create a dummy file so ISOFileSystem constructor doesn't crash immediately (though it might return early)
    // Actually, ISOFileSystem constructor takes a path. If file doesn't exist, mappedFile.open fails, and it returns.
    // This is fine, we just want the object instance.

    // Create a dummy file just in case
    // std::ofstream("dummy.iso");

    ISOFileSystem isoFs("dummy.iso");

    // Populate fileMap with many entries
    const int NUM_FILES = 100000;
    const int NUM_LOOKUPS = 1000000;

    std::cout << "Populating fileMap with " << NUM_FILES << " entries..." << std::endl;

    std::vector<std::string> filenames;
    filenames.reserve(NUM_FILES);

    for (int i = 0; i < NUM_FILES; ++i) {
        std::string name = "file_" + std::to_string(i) + ".dat";
        filenames.push_back(name);
        isoFs.fileMap[name] = {static_cast<size_t>(i * 1024), 1024};
    }

    // Benchmark lookups
    std::cout << "Benchmarking " << NUM_LOOKUPS << " lookups (exists())..." << std::endl;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, NUM_FILES - 1);

    auto start = std::chrono::high_resolution_clock::now();

    volatile size_t hits = 0;
    for (int i = 0; i < NUM_LOOKUPS; ++i) {
        int index = dist(rng);
        if (isoFs.exists(filenames[index])) {
            hits++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Time: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Average time per lookup: " << (elapsed.count() * 1000.0 / NUM_LOOKUPS) << " us" << std::endl;

    // Benchmark getSize
    std::cout << "Benchmarking " << NUM_LOOKUPS << " lookups (getSize())..." << std::endl;

    start = std::chrono::high_resolution_clock::now();

    volatile size_t totalSize = 0;
    for (int i = 0; i < NUM_LOOKUPS; ++i) {
        int index = dist(rng);
        totalSize = totalSize + isoFs.getSize(filenames[index]);
    }

    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    std::cout << "Time: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Average time per lookup: " << (elapsed.count() * 1000.0 / NUM_LOOKUPS) << " us" << std::endl;
}

int main() {
    run_benchmark();
    return 0;
}
