#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <cstring>
#include <chrono>

void sink(std::string&& s) {
    volatile size_t len = s.length();
    (void)len;
}

const int ITERATIONS = 1000000;

void benchmark_original() {
    std::string_view base = "root/directory/subdirectory/another_level/";
    const char* rawName = "example_file_name.extension";
    size_t nameLen = std::strlen(rawName);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        char fileName[256];
        memcpy(fileName, rawName, nameLen);
        fileName[nameLen] = '\0';

        std::string fileNameUTF8;
        fileNameUTF8.reserve(base.length() + nameLen);
        fileNameUTF8.append(base);
        fileNameUTF8.append(fileName, nameLen);

        bool isDir = (i % 10 == 0); // 10% directories

        if (isDir) {
            std::string s = std::move(fileNameUTF8);
            s.push_back('/');
            sink(std::move(s));
        } else {
            sink(std::move(fileNameUTF8));
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Original: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms" << std::endl;
}

void benchmark_optimized() {
    std::string_view base = "root/directory/subdirectory/another_level/";
    const char* rawName = "example_file_name.extension";
    size_t nameLen = std::strlen(rawName);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        bool isDir = (i % 10 == 0); // 10% directories

        if (isDir) {
            std::string fileNameUTF8;
            // Reserve extra for '/'
            fileNameUTF8.reserve(base.length() + nameLen + 1);
            fileNameUTF8.append(base);
            fileNameUTF8.append(rawName, nameLen);
            fileNameUTF8.push_back('/');
            sink(std::move(fileNameUTF8));
        } else {
            std::string fileNameUTF8;
            fileNameUTF8.reserve(base.length() + nameLen);
            fileNameUTF8.append(base);
            fileNameUTF8.append(rawName, nameLen);
            sink(std::move(fileNameUTF8));
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Optimized: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms" << std::endl;
}

int main() {
    benchmark_original();
    benchmark_optimized();
    return 0;
}
