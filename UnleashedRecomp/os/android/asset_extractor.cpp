#include "asset_extractor.h"

#ifdef __ANDROID__

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <SDL.h>
#include <zstd.h>
#include <os/logger.h>
#include <cstring>
#include <vector>
#include <fstream>

// Minimal tar header (POSIX ustar)
struct TarHeader
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

static_assert(sizeof(TarHeader) == 512, "Tar header must be 512 bytes");

static uint64_t ParseOctal(const char* str, size_t len)
{
    uint64_t result = 0;
    for (size_t i = 0; i < len && str[i] != '\0' && str[i] != ' '; i++)
    {
        if (str[i] >= '0' && str[i] <= '7')
            result = (result << 3) | (str[i] - '0');
    }
    return result;
}

static bool IsZeroBlock(const char* block, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (block[i] != 0)
            return false;
    }
    return true;
}

static std::string BuildFullPath(const TarHeader& header)
{
    std::string path;
    if (header.prefix[0] != '\0')
    {
        path.assign(header.prefix, strnlen(header.prefix, sizeof(header.prefix)));
        path += '/';
    }
    path.append(header.name, strnlen(header.name, sizeof(header.name)));
    return path;
}

static AAssetManager* GetAssetManager()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env)
        return nullptr;

    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity)
        return nullptr;

    jclass cls = env->GetObjectClass(activity);
    jmethodID getAssets = env->GetMethodID(cls, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assetManager = env->CallObjectMethod(activity, getAssets);

    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);

    env->DeleteLocalRef(assetManager);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);

    return mgr;
}

bool HasEmbeddedGameData()
{
    AAssetManager* mgr = GetAssetManager();
    if (!mgr)
        return false;

    // Check for chunked format (game_data_part_00.zst, ...)
    AAsset* asset = AAssetManager_open(mgr, "game_data_part_00.zst", AASSET_MODE_UNKNOWN);
    if (asset)
    {
        AAsset_close(asset);
        return true;
    }

    // Fallback: single file format (backward compatibility)
    asset = AAssetManager_open(mgr, "game_data.tar.zst", AASSET_MODE_UNKNOWN);
    if (!asset)
        return false;

    AAsset_close(asset);
    return true;
}

bool ExtractEmbeddedGameData(
    const std::filesystem::path& targetPath,
    const std::function<bool(uint64_t, uint64_t)>& progressCallback)
{
    AAssetManager* mgr = GetAssetManager();
    if (!mgr)
    {
        LOGN_ERROR("Failed to get Android AssetManager");
        return false;
    }

    // Discover available asset chunks
    std::vector<std::string> chunkNames;
    for (int i = 0; ; i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "game_data_part_%02d.zst", i);
        AAsset* test = AAssetManager_open(mgr, name, AASSET_MODE_UNKNOWN);
        if (!test)
            break;
        AAsset_close(test);
        chunkNames.push_back(name);
    }

    // Fallback: single file format (backward compatibility)
    if (chunkNames.empty())
    {
        AAsset* test = AAssetManager_open(mgr, "game_data.tar.zst", AASSET_MODE_UNKNOWN);
        if (test)
        {
            AAsset_close(test);
            chunkNames.push_back("game_data.tar.zst");
        }
    }

    if (chunkNames.empty())
    {
        LOGN("No embedded game data found in APK assets");
        return false;
    }

    // Calculate total compressed size
    uint64_t totalCompressedSize = 0;
    for (const auto& name : chunkNames)
    {
        AAsset* chunk = AAssetManager_open(mgr, name.c_str(), AASSET_MODE_UNKNOWN);
        if (chunk)
        {
            totalCompressedSize += AAsset_getLength(chunk);
            AAsset_close(chunk);
        }
    }
    LOGFN("Found {} game data chunk(s), total compressed: {} bytes", chunkNames.size(), totalCompressedSize);

    // Streaming decompression: read from asset chunks -> zstd -> tar data.
    // This avoids loading all compressed data into memory at once.
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    if (!dctx)
    {
        LOGN_ERROR("Failed to create zstd decompression context");
        return false;
    }

    constexpr size_t READ_BUF_SIZE = 4 * 1024 * 1024;  // 4MB read buffer
    constexpr size_t OUT_BUF_SIZE = 4 * 1024 * 1024;    // 4MB output buffer
    std::vector<uint8_t> readBuf(READ_BUF_SIZE);
    std::vector<uint8_t> outBuf(OUT_BUF_SIZE);
    std::vector<uint8_t> tarData;

    // Try to get decompressed size from zstd frame header for reserve hint
    {
        AAsset* firstChunk = AAssetManager_open(mgr, chunkNames[0].c_str(), AASSET_MODE_STREAMING);
        if (firstChunk)
        {
            uint8_t header[18];
            int headerRead = AAsset_read(firstChunk, header, sizeof(header));
            AAsset_close(firstChunk);
            if (headerRead > 0)
            {
                uint64_t frameSize = ZSTD_getFrameContentSize(header, headerRead);
                if (frameSize != ZSTD_CONTENTSIZE_UNKNOWN && frameSize != ZSTD_CONTENTSIZE_ERROR)
                {
                    LOGFN("Decompressed size: {} bytes", frameSize);
                    tarData.reserve(frameSize);
                }
                else
                {
                    LOGFN("Decompressed size unknown, estimating: {} bytes", totalCompressedSize * 3);
                    tarData.reserve(totalCompressedSize * 3);
                }
            }
        }
    }

    for (const auto& chunkName : chunkNames)
    {
        AAsset* chunk = AAssetManager_open(mgr, chunkName.c_str(), AASSET_MODE_STREAMING);
        if (!chunk)
        {
            LOGFN_ERROR("Failed to open chunk: {}", chunkName);
            ZSTD_freeDCtx(dctx);
            return false;
        }

        LOGFN("Processing chunk: {}", chunkName);
        while (true)
        {
            int bytesRead = AAsset_read(chunk, readBuf.data(), READ_BUF_SIZE);
            if (bytesRead <= 0)
                break;

            ZSTD_inBuffer input = { readBuf.data(), static_cast<size_t>(bytesRead), 0 };
            while (input.pos < input.size)
            {
                ZSTD_outBuffer output = { outBuf.data(), outBuf.size(), 0 };
                size_t ret = ZSTD_decompressStream(dctx, &output, &input);
                if (ZSTD_isError(ret))
                {
                    LOGFN_ERROR("Zstd decompression error: {}", ZSTD_getErrorName(ret));
                    AAsset_close(chunk);
                    ZSTD_freeDCtx(dctx);
                    return false;
                }

                tarData.insert(tarData.end(), outBuf.data(), outBuf.data() + output.pos);
            }
        }

        AAsset_close(chunk);
    }

    ZSTD_freeDCtx(dctx);

    LOGFN("Decompressed tar data: {} bytes", tarData.size());

    // Parse tar archive and extract files
    const uint64_t totalSize = tarData.size();
    uint64_t bytesProcessed = 0;
    size_t offset = 0;
    uint32_t filesExtracted = 0;
    int consecutiveZeroBlocks = 0;

    while (offset + sizeof(TarHeader) <= tarData.size())
    {
        // Check for end-of-archive marker (two consecutive zero blocks)
        if (IsZeroBlock(reinterpret_cast<const char*>(tarData.data() + offset), 512))
        {
            consecutiveZeroBlocks++;
            if (consecutiveZeroBlocks >= 2)
                break;

            offset += 512;
            continue;
        }
        consecutiveZeroBlocks = 0;

        const TarHeader& header = *reinterpret_cast<const TarHeader*>(tarData.data() + offset);
        offset += 512;

        // Validate magic
        if (strncmp(header.magic, "ustar", 5) != 0)
        {
            LOGFN_ERROR("Invalid tar header magic at offset {}", offset - 512);
            return false;
        }

        std::string relativePath = BuildFullPath(header);
        uint64_t fileSize = ParseOctal(header.size, sizeof(header.size));

        // Skip . or empty entries
        if (relativePath.empty() || relativePath == "." || relativePath == "./")
        {
            offset += ((fileSize + 511) / 512) * 512;
            bytesProcessed = offset;
            continue;
        }

        std::filesystem::path fullPath = targetPath / relativePath;

        if (header.typeflag == '5') // Directory
        {
            std::error_code ec;
            std::filesystem::create_directories(fullPath, ec);
            if (ec)
            {
                LOGFN_ERROR("Failed to create directory: {} ({})", fullPath.string(), ec.message());
            }
        }
        else if (header.typeflag == '0' || header.typeflag == '\0') // Regular file
        {
            if (offset + fileSize > tarData.size())
            {
                LOGFN_ERROR("Tar file truncated: {} needs {} bytes at offset {}",
                    relativePath, fileSize, offset);
                return false;
            }

            // Create parent directories
            std::error_code ec;
            std::filesystem::create_directories(fullPath.parent_path(), ec);

            // Write file
            std::ofstream outFile(fullPath, std::ios::binary);
            if (!outFile.is_open())
            {
                LOGFN_ERROR("Failed to create file: {}", fullPath.string());
                return false;
            }

            outFile.write(reinterpret_cast<const char*>(tarData.data() + offset), fileSize);
            outFile.close();
            filesExtracted++;

            // Advance past file data (padded to 512-byte boundary)
            offset += ((fileSize + 511) / 512) * 512;
        }
        else
        {
            // Skip unknown entry types
            offset += ((fileSize + 511) / 512) * 512;
        }

        bytesProcessed = offset;

        if (progressCallback && !progressCallback(bytesProcessed, totalSize))
        {
            LOGN("Asset extraction cancelled by user");
            return false;
        }
    }

    LOGFN("Extracted {} files from embedded game data", filesExtracted);
    return filesExtracted > 0;
}

#endif
