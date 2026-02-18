#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "install/directory_file_system.h"

namespace fs = std::filesystem;

class TempDirectoryFixture {
public:
    fs::path tempPath;

    TempDirectoryFixture() {
        tempPath = fs::temp_directory_path() / "DirectoryFileSystemTest";
        if (fs::exists(tempPath)) {
            fs::remove_all(tempPath);
        }
        fs::create_directories(tempPath);
    }

    ~TempDirectoryFixture() {
        if (fs::exists(tempPath)) {
            fs::remove_all(tempPath);
        }
    }

    void CreateFile(const std::string& filename, const std::string& content) {
        std::ofstream ofs(tempPath / filename, std::ios::binary);
        ofs << content;
        ofs.close();
    }
};

TEST_CASE("DirectoryFileSystem Basic Operations") {
    TempDirectoryFixture fixture;
    auto vfs = DirectoryFileSystem::create(fixture.tempPath);

    REQUIRE(vfs != nullptr);
    CHECK(vfs->getName() == fixture.tempPath.filename().string());

    SUBCASE("File Existence") {
        fixture.CreateFile("test.txt", "Hello");
        CHECK(vfs->exists("test.txt"));
        CHECK_FALSE(vfs->exists("nonexistent.txt"));
    }

    SUBCASE("Get Size") {
        std::string content = "Hello World";
        fixture.CreateFile("size.txt", content);
        CHECK(vfs->getSize("size.txt") == content.size());
        CHECK(vfs->getSize("nonexistent.txt") == 0);
    }

    SUBCASE("Load File") {
        std::string content = "Data";
        fixture.CreateFile("data.txt", content);

        std::vector<uint8_t> buffer;
        bool result = vfs->load("data.txt", buffer);
        CHECK(result);
        CHECK(buffer.size() == content.size());
        CHECK(std::string(buffer.begin(), buffer.end()) == content);

        // Test non-existent file
        std::vector<uint8_t> emptyBuffer;
        CHECK_FALSE(vfs->load("missing.txt", emptyBuffer));
    }

    SUBCASE("Invalid Directory") {
        auto invalidVfs = DirectoryFileSystem::create(fixture.tempPath / "NonExistentDir");
        CHECK(invalidVfs == nullptr);
    }
}
