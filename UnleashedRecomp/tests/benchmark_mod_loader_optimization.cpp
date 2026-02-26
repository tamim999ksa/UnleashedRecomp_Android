#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <ankerl/unordered_dense.h>
#include <random>

// Mock global map
using ModFileIndex = ankerl::unordered_dense::map<std::string, int>;
ModFileIndex g_modFileIndex;

// Original Implementation (Baseline)
bool Baseline(std::string_view path) {
    std::string pathStr(path);
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

    std::string key = pathStr;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });

    auto it = g_modFileIndex.find(key);
    if (it != g_modFileIndex.end()) {
        std::filesystem::path fsPath(pathStr);
        // Simulate usage
        volatile auto p = fsPath.c_str();
        (void)p;
        return true;
    }
    return false;
}

// Optimized Implementation
bool Optimized(std::string_view path) {
    thread_local std::string s_lookupKey;
    s_lookupKey.resize(path.size());

    // Transform directly from path to key (normalized + lowercase) in one pass
    // Also reusing buffer to avoid allocation
    std::transform(path.begin(), path.end(), s_lookupKey.begin(), [](char c) {
        return (c == '\\') ? '/' : std::tolower((unsigned char)c);
    });

    auto it = g_modFileIndex.find(s_lookupKey);
    if (it != g_modFileIndex.end()) {
        // Construct pathStr only if found
        std::string pathStr(path);
        std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

        std::filesystem::path fsPath(pathStr);
        volatile auto p = fsPath.c_str();
        (void)p;
        return true;
    }
    return false;
}

// Correctness Check
void VerifyCorrectness() {
    std::vector<std::string> testPaths = {
        "Data\\Sound\\BGM\\Event.csb",
        "system/shader/generic.fx",
        "Save\\SYS-DATA",
        "Mixed/Separators\\Path.ext"
    };

    // Populate map with some entries
    g_modFileIndex["data/sound/bgm/event.csb"] = 1;
    g_modFileIndex["system/shader/generic.fx"] = 2;

    for (const auto& path : testPaths) {
        bool base = Baseline(path);
        bool opt = Optimized(path);

        if (base != opt) {
            std::cerr << "Mismatch for path: " << path << " (Baseline: " << base << ", Optimized: " << opt << ")" << std::endl;
            exit(1);
        }
    }
    std::cout << "Correctness verification passed." << std::endl;
}

int main() {
    // Setup
    const int NUM_FILES = 10000;
    const int NUM_LOOKUPS = 1000000;

    std::cout << "Populating index with " << NUM_FILES << " entries..." << std::endl;
    for (int i = 0; i < NUM_FILES; ++i) {
        std::string name = "file_" + std::to_string(i) + ".dat";
        g_modFileIndex[name] = i;
    }

    // Generate lookups (mix of hits and misses)
    std::vector<std::string> queries;
    queries.reserve(NUM_LOOKUPS);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, NUM_FILES * 2); // 50% hit rate roughly

    for (int i = 0; i < NUM_LOOKUPS; ++i) {
        int idx = dist(rng);
        if (idx < NUM_FILES) {
            queries.push_back("file_" + std::to_string(idx) + ".dat");
        } else {
            queries.push_back("missing_file_" + std::to_string(idx) + ".dat");
        }

        // Randomly use backslashes to simulate real paths
        if (idx % 3 == 0) {
            std::string& s = queries.back();
            std::replace(s.begin(), s.end(), '/', '\\');
            // Note: simple replace, original names didn't have / anyway except maybe implicitly?
            // "file_X.dat" has no slash. Let's add prefix.
            s = "prefix\\subfolder\\" + s;
        } else {
             queries.back() = "prefix/subfolder/" + queries.back();
        }
    }

    // Add prefixes to map keys too so lookup works
    {
        ModFileIndex newMap;
        for (auto& [k, v] : g_modFileIndex) {
            newMap["prefix/subfolder/" + k] = v;
        }
        g_modFileIndex = std::move(newMap);
    }

    VerifyCorrectness();

    // Benchmark Baseline
    auto start = std::chrono::high_resolution_clock::now();
    volatile size_t hitsBase = 0;
    for (const auto& query : queries) {
        if (Baseline(query)) hitsBase++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsedBase = end - start;

    std::cout << "Baseline Time: " << elapsedBase.count() << " ms (" << hitsBase << " hits)" << std::endl;

    // Benchmark Optimized
    start = std::chrono::high_resolution_clock::now();
    volatile size_t hitsOpt = 0;
    for (const auto& query : queries) {
        if (Optimized(query)) hitsOpt++;
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsedOpt = end - start;

    std::cout << "Optimized Time: " << elapsedOpt.count() << " ms (" << hitsOpt << " hits)" << std::endl;

    double speedup = elapsedBase.count() / elapsedOpt.count();
    std::cout << "Speedup: " << speedup << "x" << std::endl;

    if (hitsBase != hitsOpt) {
        std::cerr << "Error: Hit count mismatch!" << std::endl;
        return 1;
    }

    return 0;
}
