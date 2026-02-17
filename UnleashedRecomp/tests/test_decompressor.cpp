#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <zstd.h>
#include "../decompressor.h"

TEST_CASE("decompressZstd") {
    const char* src = "Hello, World! This is a test string for Zstd compression.";
    size_t src_len = std::strlen(src) + 1;
    std::vector<uint8_t> compressed_vec(1024);
    size_t compressed_size = ZSTD_compress(compressed_vec.data(), compressed_vec.size(), src, src_len, 1);

    REQUIRE(!ZSTD_isError(compressed_size));

    // Create a fixed-size buffer as expected by decompressZstd
    // This template expects a reference to an array, so we must provide one.
    // Initialize with zeros to avoid undefined behavior when reading past valid data.
    uint8_t compressed_buffer[1024] = {0};
    std::memcpy(compressed_buffer, compressed_vec.data(), compressed_size);

    // Call the function under test
    auto result = decompressZstd(compressed_buffer, src_len);

    // Verify the result
    CHECK(std::memcmp(result.get(), src, src_len) == 0);
}

TEST_CASE("decompressZstd with invalid data") {
    // Fill buffer with garbage data that is not a valid zstd frame
    uint8_t invalid_data[100];
    for(size_t i = 0; i < 100; ++i) invalid_data[i] = (uint8_t)i;

    // Decompress should not crash
    // Current implementation ignores error and returns zero-initialized buffer
    auto result = decompressZstd(invalid_data, 100);

    // Verify result is not null
    REQUIRE(result != nullptr);

    // Verify content is zero-initialized (since decompression failed)
    CHECK(result[0] == 0);
    CHECK(result[99] == 0);
}
