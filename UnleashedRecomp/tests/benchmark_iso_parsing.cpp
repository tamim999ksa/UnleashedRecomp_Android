#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <random>
#include <algorithm>

#include "install/iso_file_system.h"

// Define sector size
const size_t XE_SECTOR_SIZE = 2048;

void create_benchmark_iso(const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);

    // Create a buffer for sectors
    std::vector<uint8_t> buffer(2048, 0);

    // File needs to be large enough.
    // 0x00000000 + 32 * 2048 = 65536. Magic here.
    // Root info at 65536 + 20.

    // We need sectors up to... let's say 10MB file.
    // 10MB = 5000 sectors.
    size_t file_size = 10 * 1024 * 1024;
    ofs.seekp(file_size - 1);
    ofs.write("", 1);
    ofs.seekp(0);

    // Write Magic at 65536 (Sector 32)
    size_t magic_offset = 32 * XE_SECTOR_SIZE;
    const char* magic = "MICROSOFT*XBOX*MEDIA";
    ofs.seekp(magic_offset);
    ofs.write(magic, strlen(magic));

    // Root Dir Info at magic_offset + 20
    // Root Sector: 100
    // Root Size: we will calculate later, but let's say enough for 1 entry.
    uint32_t root_sector = 100;
    uint32_t root_size = 2048;

    ofs.seekp(magic_offset + 20);
    ofs.write(reinterpret_cast<const char*>(&root_sector), 4);
    ofs.write(reinterpret_cast<const char*>(&root_size), 4);

    // Create Root Directory at Sector 100 (Offset 100 * 2048 = 204800)
    // Entry 1: "long_directory_name..." -> Sector 200
    // We need a long name to defeat SSO.
    std::string dir_name = "very_long_directory_name_to_defeat_small_string_optimization_buffer";

    struct DirEntry {
        uint16_t nodeL;
        uint16_t nodeR;
        uint32_t sector;
        uint32_t length;
        uint8_t attributes;
        uint8_t nameLength;
        std::string name;
    };

    // Create Root Directory with 10 subdirectories
    // Each subdirectory will contain 10,000 files.

    int num_subdirs = 10;
    int files_per_subdir = 10000;

    // Root directory entries
    size_t root_offset = root_sector * XE_SECTOR_SIZE;
    size_t current_root_offset = 0;

    // We place subdirs starting at sector 200.
    // Each subdir needs ~250KB = 125 sectors.
    // So spacing 150 sectors is safe.
    uint32_t current_subdir_sector = 200;

    for (int d = 0; d < num_subdirs; ++d) {
        DirEntry entry;
        std::string dname = "subdir_long_name_to_avoid_sso_" + std::to_string(d);

        entry.nodeL = 0;
        size_t entry_size = 14 + dname.length();
        size_t padding = (4 - (entry_size % 4)) % 4;
        size_t total_size = entry_size + padding;

        if (d < num_subdirs - 1) {
             entry.nodeR = (current_root_offset + total_size) / 4;
        } else {
             entry.nodeR = 0;
        }

        entry.sector = current_subdir_sector;
        entry.length = 300 * 1024; // 300KB should be enough for directory content
        entry.attributes = 0x10; // Directory
        entry.name = dname;
        entry.nameLength = (uint8_t)dname.length();

        // Write root entry
        ofs.seekp(root_offset + current_root_offset);
        ofs.write(reinterpret_cast<const char*>(&entry.nodeL), 2);
        ofs.write(reinterpret_cast<const char*>(&entry.nodeR), 2);
        ofs.write(reinterpret_cast<const char*>(&entry.sector), 4);
        ofs.write(reinterpret_cast<const char*>(&entry.length), 4);
        ofs.write(reinterpret_cast<const char*>(&entry.attributes), 1);
        ofs.write(reinterpret_cast<const char*>(&entry.nameLength), 1);
        ofs.write(entry.name.c_str(), entry.nameLength);

        // Now fill the subdirectory
        size_t subdir_offset = current_subdir_sector * XE_SECTOR_SIZE;
        size_t current_file_offset = 0;

        for (int i = 0; i < files_per_subdir; ++i) {
            DirEntry fentry;
            fentry.nodeL = 0;
            std::string fname = "file" + std::to_string(i);
            size_t f_size = 14 + fname.length();
            size_t f_padding = (4 - (f_size % 4)) % 4;
            size_t f_total = f_size + f_padding;

            if (i < files_per_subdir - 1) {
                fentry.nodeR = (current_file_offset + f_total) / 4;
            } else {
                fentry.nodeR = 0;
            }

            fentry.sector = 0; // dummy
            fentry.length = 100;
            fentry.attributes = 0; // File
            fentry.name = fname;
            fentry.nameLength = (uint8_t)fname.length();

            ofs.seekp(subdir_offset + current_file_offset);
            ofs.write(reinterpret_cast<const char*>(&fentry.nodeL), 2);
            ofs.write(reinterpret_cast<const char*>(&fentry.nodeR), 2);
            ofs.write(reinterpret_cast<const char*>(&fentry.sector), 4);
            ofs.write(reinterpret_cast<const char*>(&fentry.length), 4);
            ofs.write(reinterpret_cast<const char*>(&fentry.attributes), 1);
            ofs.write(reinterpret_cast<const char*>(&fentry.nameLength), 1);
            ofs.write(fentry.name.c_str(), fentry.nameLength);

            current_file_offset += f_total;
        }

        current_root_offset += total_size;
        current_subdir_sector += 150;
    }

    ofs.close();
}

int main() {
    const std::string filename = "benchmark.iso";
    create_benchmark_iso(filename);

    size_t file_count = 0;
    auto start = std::chrono::high_resolution_clock::now();

    {
        ISOFileSystem iso(filename);
        // Ensure it loaded something
        if (iso.empty()) {
            std::cerr << "Failed to load ISO!" << std::endl;
            return 1;
        }
        file_count = iso.fileMap.size();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "ISO Parsing Time: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Files loaded: " << file_count << std::endl;

    std::filesystem::remove(filename);
    return 0;
}
