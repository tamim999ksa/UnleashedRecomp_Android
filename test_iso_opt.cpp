#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <cstring>
#include <chrono>
#include <stack>
#include <unordered_map>
#include <tuple>
#include <unordered_set>

// Mocking required parts of ISOFileSystem
struct ISOFileSystem {
    std::unordered_map<std::string, std::tuple<size_t, size_t>> fileMap;
    // ... other members mocked or ignored

    // Original implementation logic recreated for benchmark
    void parseDirectoryTree_Original(int iterations) {
        struct IterationStep
        {
            std::string_view fileNameBase;
            size_t nodeOffset = 0;
            size_t entryOffset = 0;

            IterationStep() = default;
            IterationStep(std::string_view fileNameBase, size_t nodeOffset, size_t entryOffset) : fileNameBase(fileNameBase), nodeOffset(nodeOffset), entryOffset(entryOffset) { }
        };

        std::deque<std::string> pathCache;
        pathCache.emplace_back("");

        std::stack<IterationStep> iterationStack;
        iterationStack.emplace(pathCache.back(), 0, 0);

        char fileName[256];
        const char* rawName = "test_file_name_entry";
        size_t rawNameLen = std::strlen(rawName);

        // Populate fileName buffer
        std::memcpy(fileName, rawName, rawNameLen);
        fileName[rawNameLen] = '\0';
        uint8_t nameLength = (uint8_t)rawNameLen;

        const uint8_t FileAttributeDirectory = 0x10;

        for(int i=0; i<iterations; ++i) {
             // Mock loop state
             std::string_view base = pathCache.back();
             uint8_t attributes = (i % 10 == 0) ? FileAttributeDirectory : 0;
             uint32_t length = (attributes & FileAttributeDirectory) ? 100 : 500;
             size_t sector = i * 10;
             size_t gameOffset = 0;
             size_t XeSectorSize = 2048;

             // ORIGINAL CODE BLOCK START
             std::string fileNameUTF8;
             fileNameUTF8.reserve(base.length() + nameLength);
             fileNameUTF8.append(base);
             fileNameUTF8.append(fileName, nameLength);

             if (attributes & FileAttributeDirectory)
             {
                 if (length > 0)
                 {
                     pathCache.emplace_back(std::move(fileNameUTF8)).push_back('/');
                     // iterationStack.emplace(pathCache.back(), gameOffset + sector * XeSectorSize, 0); // Mocked out
                 }
             }
             else
             {
                 fileMap.try_emplace(std::move(fileNameUTF8), gameOffset + sector * XeSectorSize, length);
             }
             // ORIGINAL CODE BLOCK END

             // Cleanup for loop to prevent infinite growth during benchmark if needed
             if (pathCache.size() > 10) pathCache.pop_back();
        }
    }

    // Optimized implementation
    void parseDirectoryTree_Optimized(int iterations) {
        std::deque<std::string> pathCache;
        pathCache.emplace_back("");

        char fileName[256];
        const char* rawName = "test_file_name_entry";
        size_t rawNameLen = std::strlen(rawName);
        std::memcpy(fileName, rawName, rawNameLen);
        fileName[rawNameLen] = '\0';
        uint8_t nameLength = (uint8_t)rawNameLen;

        const uint8_t FileAttributeDirectory = 0x10;

        for(int i=0; i<iterations; ++i) {
             std::string_view base = pathCache.back();
             uint8_t attributes = (i % 10 == 0) ? FileAttributeDirectory : 0;
             uint32_t length = (attributes & FileAttributeDirectory) ? 100 : 500;
             size_t sector = i * 10;
             size_t gameOffset = 0;
             size_t XeSectorSize = 2048;

             bool isDirectory = (attributes & FileAttributeDirectory) && (length > 0);

             // OPTIMIZED CODE BLOCK START
             std::string fileNameUTF8;
             fileNameUTF8.reserve(base.length() + nameLength + (isDirectory ? 1 : 0));
             fileNameUTF8.append(base);
             fileNameUTF8.append(fileName, nameLength);

             if (isDirectory)
             {
                 fileNameUTF8.push_back('/');
                 pathCache.emplace_back(std::move(fileNameUTF8));
                 // iterationStack.emplace(pathCache.back(), gameOffset + sector * XeSectorSize, 0);
             }
             else
             {
                 fileMap.try_emplace(std::move(fileNameUTF8), gameOffset + sector * XeSectorSize, length);
             }
             // OPTIMIZED CODE BLOCK END

             if (pathCache.size() > 10) pathCache.pop_back();
        }
    }
};

int main() {
    const int iterations = 1000000;

    {
        ISOFileSystem iso;
        auto start = std::chrono::high_resolution_clock::now();
        iso.parseDirectoryTree_Original(iterations);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Original: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    }

    {
        ISOFileSystem iso;
        auto start = std::chrono::high_resolution_clock::now();
        iso.parseDirectoryTree_Optimized(iterations);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Optimized: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    }

    return 0;
}
