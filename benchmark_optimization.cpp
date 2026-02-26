#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <cstring>
#include <chrono>
#include <stack>
#include <unordered_map>

// Mock map to simulate behavior
std::unordered_map<std::string, int> fileMap;
std::deque<std::string> pathCache;

const int ITERATIONS = 100000;
const char* RAW_DATA = "some_long_filename_string_to_simulate_memory_mapped_file";

struct IterationStep {
    std::string_view fileNameBase;
    int depth;
};

void sink(std::string&& s) {
    volatile size_t len = s.length();
    (void)len;
}

void benchmark_baseline() {
    fileMap.clear();
    pathCache.clear();
    pathCache.emplace_back("");

    std::string_view base = pathCache.back();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        // Simulate reading form memory mapped file
        char fileName[256];
        size_t nameLength = 15;
        memcpy(fileName, RAW_DATA, nameLength);
        fileName[nameLength] = '\0';

        std::string fileNameUTF8;
        fileNameUTF8.reserve(base.length() + nameLength);
        fileNameUTF8.append(base);
        fileNameUTF8.append(fileName, nameLength);

        bool isDir = (i % 10 == 0);

        if (isDir) {
             // Simulate directory handling
             std::string s = std::move(fileNameUTF8);
             pathCache.emplace_back(std::move(s));
             pathCache.back().push_back('/');
             // Simulate getting reference for next step
             volatile auto& ref = pathCache.back();
             (void)ref;
        } else {
             // Simulate file handling
             fileMap.try_emplace(std::move(fileNameUTF8), i);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Baseline: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms" << std::endl;
}

void benchmark_optimized() {
    fileMap.clear();
    pathCache.clear();
    pathCache.emplace_back("");

    std::string_view base = pathCache.back();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        size_t nameLength = 15;
        const char* namePtr = RAW_DATA; // Direct pointer

        bool isDir = (i % 10 == 0);

        std::string fileNameUTF8;
        // Optimization 1: Reserve extra byte for directory
        fileNameUTF8.reserve(base.length() + nameLength + (isDir ? 1 : 0));
        fileNameUTF8.append(base);
        // Optimization 2: Append directly
        fileNameUTF8.append(namePtr, nameLength);

        if (isDir) {
             fileNameUTF8.push_back('/');
             pathCache.emplace_back(std::move(fileNameUTF8));
        } else {
             fileMap.try_emplace(std::move(fileNameUTF8), i);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Optimized: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms" << std::endl;
}

// Optimization 3: Reuse buffer?
void benchmark_reuse() {
    fileMap.clear();
    pathCache.clear();
    pathCache.emplace_back("");

    std::string_view base = pathCache.back();

    std::string buffer;
    buffer.reserve(256);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        size_t nameLength = 15;
        const char* namePtr = RAW_DATA;

        bool isDir = (i % 10 == 0);

        buffer.clear(); // retain capacity
        // Ensure capacity
        size_t needed = base.length() + nameLength + (isDir ? 1 : 0);
        if (buffer.capacity() < needed) buffer.reserve(needed);

        buffer.append(base);
        buffer.append(namePtr, nameLength);

        if (isDir) {
             buffer.push_back('/');
             pathCache.emplace_back(buffer); // COPY
        } else {
             fileMap.try_emplace(buffer, i); // COPY
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Reuse+Copy: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms" << std::endl;
}


int main() {
    benchmark_baseline();
    benchmark_optimized();
    benchmark_reuse();
    return 0;
}
