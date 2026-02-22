#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <xex.h>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cassert>

#define PPC_MEMORY_SIZE 0x100000000ull

struct Memory
{
    uint8_t* base;
    size_t size;

    Memory(size_t s = PPC_MEMORY_SIZE) : size(s)
    {
        base = new uint8_t[4096];
    }

    ~Memory() { delete[] base; }

    void* Translate(uint32_t offset) const noexcept
    {
        return base + offset;
    }
};

Memory g_memory;

bool LoadXexImageSafe(const Xex2SecurityInfo* security, const Xex2OptFileFormatInfo* fileFormatInfo, const uint8_t* srcData)
{
    uint32_t loadAddress = security->loadAddress;
    uint32_t imageSize = security->imageSize;

    if ((uint64_t)loadAddress + imageSize > PPC_MEMORY_SIZE)
    {
        return false;
    }

    auto destData = reinterpret_cast<uint8_t*>(g_memory.Translate(loadAddress));

    if (fileFormatInfo->compressionType == XEX_COMPRESSION_NONE)
    {
        return true;
    }
    else if (fileFormatInfo->compressionType == XEX_COMPRESSION_BASIC)
    {
        if (fileFormatInfo->infoSize < sizeof(Xex2FileBasicCompressionInfo))
        {
            return false;
        }

        auto* blocks = reinterpret_cast<const Xex2FileBasicCompressionBlock*>(fileFormatInfo + 1);
        const size_t numBlocks = (fileFormatInfo->infoSize / sizeof(Xex2FileBasicCompressionInfo)) - 1;

        uint8_t* currentDest = destData;

        for (size_t i = 0; i < numBlocks; i++)
        {
            uint32_t dataSize = blocks[i].dataSize;
            uint32_t zeroSize = blocks[i].zeroSize;

            size_t currentOffset = currentDest - g_memory.base;

            // Check overflow using 64-bit arithmetic
            if ((uint64_t)currentOffset + dataSize > PPC_MEMORY_SIZE) return false;

            // Increment
            currentDest += dataSize;

            if ((uint64_t)currentOffset + dataSize + zeroSize > PPC_MEMORY_SIZE) return false;
            currentDest += zeroSize;
        }
        return true;
    }

    return false;
}

TEST_CASE("Test LoadXexImageSafe Security Checks")
{
    Xex2SecurityInfo security;
    Xex2OptFileFormatInfo formatInfo;
    uint8_t dummySrc[100];

    security.loadAddress = 0;
    security.imageSize = 0;

    SUBCASE("Valid Uncompressed Image")
    {
        security.loadAddress = 0x100000;
        security.imageSize = 0x1000;
        formatInfo.compressionType = XEX_COMPRESSION_NONE;

        CHECK(LoadXexImageSafe(&security, &formatInfo, dummySrc) == true);
    }

    SUBCASE("Image Size Overflow")
    {
        security.loadAddress = 0xFFFFFF00;
        security.imageSize = 0x200;
        formatInfo.compressionType = XEX_COMPRESSION_NONE;

        CHECK(LoadXexImageSafe(&security, &formatInfo, dummySrc) == false);
    }

    SUBCASE("Basic Compression - Valid")
    {
        security.loadAddress = 0x100000;
        security.imageSize = 0x2000;

        struct {
            Xex2OptFileFormatInfo info;
            Xex2FileBasicCompressionBlock block;
        } data;

        data.info.compressionType = XEX_COMPRESSION_BASIC;
        data.info.infoSize = sizeof(Xex2FileBasicCompressionInfo) * 2;
        data.block.dataSize = 0x100;
        data.block.zeroSize = 0x100;

        CHECK(LoadXexImageSafe(&security, &data.info, dummySrc) == true);
    }

    SUBCASE("Basic Compression - Info Size Underflow")
    {
        security.loadAddress = 0x100000;
        formatInfo.compressionType = XEX_COMPRESSION_BASIC;
        formatInfo.infoSize = 4;

        CHECK(LoadXexImageSafe(&security, &formatInfo, dummySrc) == false);
    }

    SUBCASE("Basic Compression - Integer Overflow in Block Size")
    {
        security.loadAddress = 0x100000;

        struct {
            Xex2OptFileFormatInfo info;
            Xex2FileBasicCompressionBlock block;
        } data;

        data.info.compressionType = XEX_COMPRESSION_BASIC;
        data.info.infoSize = sizeof(Xex2FileBasicCompressionInfo) * 2;
        data.block.dataSize = 0xFFFFFFFF;
        data.block.zeroSize = 1;

        CHECK(LoadXexImageSafe(&security, &data.info, dummySrc) == false);
    }
}
