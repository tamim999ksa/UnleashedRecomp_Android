#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

// Adjust paths based on where this file is compiled from relative to project root
// For simplicity in this standalone benchmark, we assume include paths are set correctly
// or we use relative paths.
#include "../thirdparty/picosha2/picosha2.h"

// Mocking fmt for the benchmark if strictly needed, or just use std::cout
// But let's try to include it if we can find it.
// #include <fmt/format.h>

using FileHash = std::array<uint8_t, 32>;

// Old implementation simulation
bool checkFile_Old(const std::filesystem::path& filePath, FileHash& outHash) {
    std::ifstream fileStream(filePath, std::ios::binary);
    if (!fileStream.is_open()) return false;

    std::error_code ec;
    size_t fileSize = std::filesystem::file_size(filePath, ec);
    if (ec) return false;

    std::vector<uint8_t> fileData;
    try {
        fileData.resize(fileSize);
    } catch (const std::bad_alloc&) {
        return false;
    }

    fileStream.read((char*)(fileData.data()), fileSize);
    if (fileStream.bad()) return false;

    picosha2::hash256(fileData.data(), fileData.data() + fileSize,
                      outHash.begin(), outHash.end());
    return true;
}

// New implementation simulation
bool checkFile_New(const std::filesystem::path& filePath, FileHash& outHash) {
    std::ifstream fileStream(filePath, std::ios::binary);
    if (!fileStream.is_open()) return false;

    // Use picosha2 hash256 stream overload or one_by_one manually
    // picosha2.h has:
    // template <typename OutIter> void hash256(std::ifstream& f, OutIter first, OutIter last)

    // Using the helper directly which does exactly what we want:
    // It uses a 1MB buffer (PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR = 1048576)

    picosha2::hash256(fileStream, outHash.begin(), outHash.end());

    return true;
}

void printHash(const std::string& label, const FileHash& hash) {
    std::cout << label << ": ";
    for (uint8_t b : hash) {
        printf("%02x", b);
    }
    std::cout << std::endl;
}

int main() {
    const std::string testFileName = "temp_benchmark_file.bin";
    const size_t testFileSize = 100 * 1024 * 1024; // 100 MB

    std::cout << "Generating 100MB test file..." << std::endl;
    {
        std::ofstream out(testFileName, std::ios::binary);
        std::vector<char> buffer(1024 * 1024);
        std::mt19937 gen(12345);
        std::uniform_int_distribution<> dis(0, 255);

        // Fill buffer with random data
        for (auto& b : buffer) b = static_cast<char>(dis(gen));

        for (size_t i = 0; i < testFileSize / buffer.size(); ++i) {
            out.write(buffer.data(), buffer.size());
        }
    }

    std::cout << "Benchmarking..." << std::endl;

    // Benchmark Old
    FileHash hashOld;
    auto startOld = std::chrono::high_resolution_clock::now();
    bool successOld = checkFile_Old(testFileName, hashOld);
    auto endOld = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diffOld = endOld - startOld;

    if (successOld) {
        std::cout << "Old Method Time: " << diffOld.count() << " s" << std::endl;
        std::cout << "Old Method Allocation: " << testFileSize << " bytes" << std::endl;
        printHash("Old Hash", hashOld);
    } else {
        std::cout << "Old Method Failed" << std::endl;
    }

    // Benchmark New
    FileHash hashNew;
    auto startNew = std::chrono::high_resolution_clock::now();
    bool successNew = checkFile_New(testFileName, hashNew);
    auto endNew = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diffNew = endNew - startNew;

    if (successNew) {
        std::cout << "New Method Time: " << diffNew.count() << " s" << std::endl;
        std::cout << "New Method Allocation: ~1048576 bytes (Buffer)" << std::endl;
        printHash("New Hash", hashNew);
    } else {
        std::cout << "New Method Failed" << std::endl;
    }

    // Validation
    if (hashOld == hashNew) {
        std::cout << "SUCCESS: Hashes match!" << std::endl;
    } else {
        std::cout << "FAILURE: Hashes do not match!" << std::endl;
    }

    // Clean up
    std::filesystem::remove(testFileName);

    return 0;
}
