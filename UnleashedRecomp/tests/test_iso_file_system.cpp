#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>

#include "install/iso_file_system.h"

namespace fs = std::filesystem;

class ISOTestFixture {
public:
    fs::path tempPath;
    fs::path isoPath;
    const size_t XE_SECTOR_SIZE = 2048;

    ISOTestFixture() {
        tempPath = fs::temp_directory_path() / "ISOFileSystemTest";
        if (fs::exists(tempPath)) {
            fs::remove_all(tempPath);
        }
        fs::create_directories(tempPath);
        isoPath = tempPath / "test.iso";
    }

    ~ISOTestFixture() {
        if (fs::exists(tempPath)) {
            fs::remove_all(tempPath);
        }
    }

    void CreateValidISO(const std::string& filename, const std::string& internalFileName, const std::string& content) {
        std::ofstream ofs(filename, std::ios::binary);

        // Zero out initial space up to reasonable size
        // 201 sectors * 2048. Sector 200 is used for file content.
        std::vector<uint8_t> zeros(2048, 0);
        for(int i=0; i<300; ++i) ofs.write((char*)zeros.data(), 2048);

        // Write Magic
        size_t magic_offset = 32 * XE_SECTOR_SIZE;
        const char* magic = "MICROSOFT*XBOX*MEDIA";
        ofs.seekp(magic_offset);
        ofs.write(magic, strlen(magic));

        // Root Dir Info at magic_offset + 20
        uint32_t root_sector = 100;
        uint32_t root_size = 2048;
        ofs.seekp(magic_offset + 20);
        ofs.write((char*)&root_sector, 4);
        ofs.write((char*)&root_size, 4);

        // Create Root Directory Entry for our file
        // Entry format:
        // u16 nodeL, u16 nodeR, u32 sector, u32 length, u8 attr, u8 nameLen, name...

        struct DirEntry {
            uint16_t nodeL = 0;
            uint16_t nodeR = 0;
            uint32_t sector = 0;
            uint32_t length = 0;
            uint8_t attributes = 0;
            uint8_t nameLength = 0;
            std::string name;
        };

        DirEntry entry;
        entry.sector = 200;
        entry.length = (uint32_t)content.size();
        entry.attributes = 0; // File
        entry.name = internalFileName;
        entry.nameLength = (uint8_t)internalFileName.size();

        // Write entry to Root Sector (100)
        size_t root_offset = root_sector * XE_SECTOR_SIZE;
        ofs.seekp(root_offset);
        ofs.write((char*)&entry.nodeL, 2);
        ofs.write((char*)&entry.nodeR, 2);
        ofs.write((char*)&entry.sector, 4);
        ofs.write((char*)&entry.length, 4);
        ofs.write((char*)&entry.attributes, 1);
        ofs.write((char*)&entry.nameLength, 1);
        ofs.write(entry.name.c_str(), entry.nameLength);

        // Write file content to Sector 200
        size_t file_offset = 200 * XE_SECTOR_SIZE;
        ofs.seekp(file_offset);
        ofs.write(content.c_str(), content.size());

        ofs.close();
    }

    void CreateInvalidISO(const std::string& filename) {
        std::ofstream ofs(filename, std::ios::binary);
        std::vector<uint8_t> zeros(2048, 0);
        // Just some zeros, no magic
        for(int i=0; i<100; ++i) ofs.write((char*)zeros.data(), 2048);
        ofs.close();
    }
};

TEST_CASE("ISOFileSystem Tests") {
    ISOTestFixture fixture;

    SUBCASE("Valid ISO Load") {
        std::string content = "Hello ISO World";
        std::string internalName = "readme.txt";
        fixture.CreateValidISO(fixture.isoPath.string(), internalName, content);

        auto iso = ISOFileSystem::create(fixture.isoPath);
        REQUIRE(iso != nullptr);
        CHECK_FALSE(iso->empty());
        CHECK(iso->getName() == fixture.isoPath.filename().string());

        // Check if file exists
        CHECK(iso->exists(internalName));
        CHECK(iso->getSize(internalName) == content.size());

        // Load file content
        std::vector<uint8_t> buffer(content.size());
        CHECK(iso->load(internalName, buffer.data(), buffer.size()));
        std::string loadedContent(buffer.begin(), buffer.end());
        CHECK(loadedContent == content);

        // Check non-existent file
        CHECK_FALSE(iso->exists("nonexistent.file"));
        CHECK(iso->getSize("nonexistent.file") == 0);

        // Check buffer size too small
        std::vector<uint8_t> smallBuffer(content.size() - 1);
        CHECK_FALSE(iso->load(internalName, smallBuffer.data(), smallBuffer.size()));
    }

    SUBCASE("Invalid ISO Load (Missing Magic)") {
        fixture.CreateInvalidISO(fixture.isoPath.string());
        // Should return nullptr or empty
        auto iso = ISOFileSystem::create(fixture.isoPath);
        CHECK(iso == nullptr);
    }

    SUBCASE("File Not Found") {
        // Test with non-existent ISO file on disk
        auto iso = ISOFileSystem::create(fixture.tempPath / "missing.iso");
        CHECK(iso == nullptr);
    }
}
