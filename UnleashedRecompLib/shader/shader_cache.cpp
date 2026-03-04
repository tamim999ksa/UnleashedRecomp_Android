#include "shader_cache.h"
#include <cstdint>
#include <cstddef>

ShaderCacheEntry g_shaderCacheEntries[] = {
    { 0, 0, 0, 0, 0, 0, nullptr }
};
const size_t g_shaderCacheEntryCount = 0;

const uint8_t g_compressedDxilCache[] = { 0 };
const size_t g_dxilCacheCompressedSize = 0;
const size_t g_dxilCacheDecompressedSize = 0;

const uint8_t g_compressedSpirvCache[] = { 0 };
const size_t g_spirvCacheCompressedSize = 0;
const size_t g_spirvCacheDecompressedSize = 0;
