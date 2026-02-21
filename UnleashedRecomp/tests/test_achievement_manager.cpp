#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Define SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#include <SDL.h>

// Standard headers required by Config and Memory
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <functional>
#include <chrono>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>

// External libraries
#include <fmt/core.h>
#include <toml++/toml.hpp>

// Define PPC_MEMORY_SIZE before including memory.h
#define PPC_MEMORY_SIZE 0x100000000ull

// Define PPCFunc and PPC_LOOKUP_FUNC required by memory.h
using PPCFunc = void(void*);
#define PPC_LOOKUP_FUNC(base, guest) (*(PPCFunc**)((uint8_t*)base + guest))

// Mock Macros - Include logger first, then override
#include <os/logger.h>
#undef LOGN
#undef LOGN_WARNING
#undef LOGN_ERROR

#define LOGN(...)
#define LOGN_WARNING(...)
#define LOGN_ERROR(...)

// Mock Localise
#include <string_view>
std::string& Localise(const std::string_view& key) {
    static std::string result;
    result = std::string(key) + "_LOCALISED";
    return result;
}

// Mock ModLoader
#include "mod/mod_loader.h"
// Members are static inline, so no definition needed here.

std::filesystem::path ModLoader::ResolvePath(std::string_view path) {
    return path;
}

std::vector<std::filesystem::path>* ModLoader::GetIncludeDirectories(size_t modIndex) {
    return nullptr;
}

void ModLoader::Init() {}

// Mock Paths
// We need to implement GetUserPath and CheckPortable as they are used by inline functions in paths.h
std::filesystem::path g_testUserPath = "test_user_data";

const std::filesystem::path& GetUserPath() {
    return g_testUserPath;
}

std::filesystem::path g_executableRoot = ".";

bool CheckPortable() { return false; }

std::filesystem::path BuildUserPath() { return g_testUserPath; }

// Mock AchievementOverlay
#include "ui/achievement_overlay.h"
// Members are static inline.

static int g_lastOpenedAchievementId = -1;

void AchievementOverlay::Init() {}
void AchievementOverlay::Draw() {}
void AchievementOverlay::Open(int id) {
    g_lastOpenedAchievementId = id;
}
void AchievementOverlay::Close() {}

// Include user/config.h to get Config definitions and declarations
// This includes config_def.h internally correctly
#include "user/config.h"

// Define g_configDefinitions (declared extern in config.h)
std::vector<IConfigDef*> g_configDefinitions;

// Define Config::AchievementNotifications static member
ConfigDef<bool> Config::AchievementNotifications("System", "AchievementNotifications", true);

// Include Memory header
#include "kernel/memory.h"

// Define g_memory
Memory g_memory;

// Implement Memory constructor
Memory::Memory() {
    // Reserve 4GB address space
    void* ptr = mmap(nullptr, PPC_MEMORY_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap reserved");
        std::exit(1);
    }
    base = (uint8_t*)ptr;

    // Commit memory for the range 0x83360000 - 0x83370000 (covers 0x833647C5, etc.)
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t start_addr = (uintptr_t)base + 0x83360000;
    start_addr &= ~(page_size - 1); // Align down to page boundary

    // Allocate 128KB (should be enough)
    size_t length = 0x20000;

    if (mmap((void*)start_addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED) {
        perror("mmap commit");
        std::exit(1);
    }
}

extern "C" void* MmGetHostAddress(uint32_t ptr) {
    return g_memory.Translate(ptr);
}

// Include the source file under test
#include "user/achievement_data.cpp"
#include "user/achievement_manager.cpp"

// Helpers for testing
void CleanupTestData() {
    if (std::filesystem::exists(g_testUserPath)) {
        std::filesystem::remove_all(g_testUserPath);
    }
    std::filesystem::create_directories(g_testUserPath / "save");
    AchievementManager::Reset();
    g_lastOpenedAchievementId = -1;
}

TEST_CASE("AchievementManager Basics") {
    CleanupTestData();

    SUBCASE("Initial State") {
        CHECK(AchievementManager::GetTotalRecords() == 0);
        CHECK(AchievementManager::IsUnlocked(1) == false);
        CHECK(AchievementManager::GetTimestamp(1) == 0);
    }

    SUBCASE("Unlock Achievement") {
        AchievementManager::Unlock(10);

        CHECK(AchievementManager::IsUnlocked(10) == true);
        CHECK(AchievementManager::GetTotalRecords() == 1);
        CHECK(AchievementManager::GetTimestamp(10) != 0);
        CHECK(g_lastOpenedAchievementId == 10);

        // Unlock same achievement again should verify it remains unlocked
        time_t ts = AchievementManager::GetTimestamp(10);
        AchievementManager::Unlock(10);
        CHECK(AchievementManager::IsUnlocked(10) == true);
        CHECK(AchievementManager::GetTimestamp(10) == ts); // Timestamp shouldn't change
    }

    SUBCASE("Multiple Achievements") {
        AchievementManager::Unlock(1);
        AchievementManager::Unlock(2);
        AchievementManager::Unlock(5);

        CHECK(AchievementManager::GetTotalRecords() == 3);
        CHECK(AchievementManager::IsUnlocked(1));
        CHECK(AchievementManager::IsUnlocked(2));
        CHECK(AchievementManager::IsUnlocked(5));
        CHECK(!AchievementManager::IsUnlocked(3));
    }
}

TEST_CASE("AchievementManager Persistence") {
    CleanupTestData();

    // Unlock some achievements
    AchievementManager::Unlock(100);
    AchievementManager::Unlock(200);

    // Save
    CHECK(AchievementManager::SaveBinary(true));

    // Reset memory state
    AchievementManager::Reset();
    CHECK(AchievementManager::GetTotalRecords() == 0);

    // Load
    CHECK(AchievementManager::LoadBinary());

    // Verify restored state
    CHECK(AchievementManager::GetTotalRecords() == 2);
    CHECK(AchievementManager::IsUnlocked(100));
    CHECK(AchievementManager::IsUnlocked(200));

    // Check timestamps are preserved
    // (Note: resolution might be seconds, so we check non-zero)
    CHECK(AchievementManager::GetTimestamp(100) != 0);
}

TEST_CASE("AchievementManager Reset") {
    CleanupTestData();

    AchievementManager::Unlock(1);
    CHECK(AchievementManager::IsUnlocked(1));

    // Access memory locations that Reset writes to, to verify they are writable
    // 0x833647C5
    bool* smackdown = (bool*)g_memory.Translate(0x833647C5);
    *smackdown = true;

    AchievementManager::Reset();

    CHECK(AchievementManager::GetTotalRecords() == 0);
    CHECK(!AchievementManager::IsUnlocked(1));

    // Check if memory was reset
    CHECK(*smackdown == false);
}

TEST_CASE("AchievementManager Corruption Handling") {
    CleanupTestData();

    AchievementManager::Unlock(1);
    AchievementManager::SaveBinary(true);

    std::filesystem::path path = AchievementManager::GetDataPath(true);

    SUBCASE("Bad File Size") {
        // Truncate file
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "bad";
        file.close();

        AchievementManager::Reset();
        bool success = AchievementManager::LoadBinary();
        CHECK(success == false);
        CHECK(AchievementManager::BinStatus == EAchBinStatus::BadFileSize);
    }

    SUBCASE("Bad Signature") {
        // Corrupt signature
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(0);
        file.write("BAD ", 4);
        file.close();

        AchievementManager::Reset();
        bool success = AchievementManager::LoadBinary();
        CHECK(success == false);
        CHECK(AchievementManager::BinStatus == EAchBinStatus::BadSignature);
    }
}
