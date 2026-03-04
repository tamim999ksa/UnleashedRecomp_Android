#pragma once

#include <memory>
#include <zstd.h>
#include <cassert>
#include <cstdint>
#include <cstddef>

template<size_t N>
inline std::unique_ptr<uint8_t[]> decompressZstd(const char(&data)[N], size_t decompressedSize)
{
    auto decompressedData = std::make_unique<uint8_t[]>(decompressedSize);
    size_t result = ZSTD_decompress(decompressedData.get(), decompressedSize, (const void*)data, N - 1);
    if (ZSTD_isError(result)) {
        return nullptr;
    }
    return decompressedData;
}

template<size_t N>
inline std::unique_ptr<uint8_t[]> decompressZstd(const uint8_t(&data)[N], size_t decompressedSize)
{
    auto decompressedData = std::make_unique<uint8_t[]>(decompressedSize);
    size_t result = ZSTD_decompress(decompressedData.get(), decompressedSize, (const void*)data, N);
    if (ZSTD_isError(result)) {
        return nullptr;
    }
    return decompressedData;
}
