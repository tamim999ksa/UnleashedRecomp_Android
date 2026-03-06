#include <fstream>
#include <filesystem>
#include <vector>
#include <map>
#include <atomic>
#include <execution>
#include <algorithm>
#include <memory>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/format.h>
#include <xxhash.h>
#include <zstd.h>
#include "shader.h"
#include "shader_recompiler.h"
#include "dxc_compiler.h"
#include <smolv.h>
#include <cassert>

static std::unique_ptr<uint8_t[]> readAllBytes(const char* filePath, size_t& fileSize)
{
    FILE* file = fopen(filePath, "rb");
    if (!file) return nullptr;
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    auto data = std::make_unique<uint8_t[]>(fileSize);
    if (fread(data.get(), 1, fileSize, file) != fileSize) { fclose(file); return nullptr; }
    fclose(file);
    return data;
}

static void writeAllBytes(const char* filePath, const void* data, size_t dataSize)
{
    FILE* file = fopen(filePath, "wb");
    if (!file) return;
    fwrite(data, 1, dataSize, file);
    fclose(file);
}

static void writeByteArray(std::ostream& out, const std::string& name, const std::vector<uint8_t>& data)
{
    fmt::print(out, "extern const char {}[] = ", name);
    fmt::print(out, "\n\t\"");
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (i > 0 && i % 4096 == 0) fmt::print(out, "\"\n\t\"");
        fmt::print(out, "\\x{:02x}", data[i]);
    }
    fmt::print(out, "\";\n");
}

struct RecompiledShader
{
    std::vector<uint8_t> shaderData;
    const uint8_t* data = nullptr;
    IDxcBlob* dxil = nullptr;
    std::vector<uint8_t> spirv;
    uint32_t specConstantsMask = 0;
};

int main(int argc, char** argv)
{
#ifndef XENOS_RECOMP_INPUT
    if (argc < 4)
    {
        printf("Usage: XenosRecomp [input path] [output path] [shader common header file path]\n");
        return 0;
    }
#endif

    const char* input =
#ifdef XENOS_RECOMP_INPUT
        XENOS_RECOMP_INPUT
#else
        argv[1]
#endif
    ;

    const char* output =
#ifdef XENOS_RECOMP_OUTPUT
        XENOS_RECOMP_OUTPUT
#else
        argv[2]
#endif
        ;

    const char* includeInput =
#ifdef XENOS_RECOMP_INCLUDE_INPUT
        XENOS_RECOMP_INCLUDE_INPUT
#else
        argv[3]
#endif
        ;

    size_t includeSize = 0;
    auto includeData = readAllBytes(includeInput, includeSize);
    if (!includeData) {
        fmt::println(stderr, "Error: Could not read include file: {}", includeInput);
        return 1;
    }
    std::string_view include(reinterpret_cast<const char*>(includeData.get()), includeSize);

    if (std::filesystem::is_directory(input))
    {
        std::map<XXH64_hash_t, RecompiledShader> shaders;

        for (auto& file : std::filesystem::recursive_directory_iterator(input))
        {
            if (std::filesystem::is_directory(file)) continue;

            size_t fileSize = 0;
            auto fileData = readAllBytes(file.path().string().c_str(), fileSize);
            if (!fileData) continue;

            for (size_t i = 0; fileSize >= sizeof(ShaderContainer) && i <= fileSize - sizeof(ShaderContainer);)
            {
                auto shaderContainer = reinterpret_cast<const ShaderContainer*>(fileData.get() + i);
                size_t dataSize = shaderContainer->virtualSize + shaderContainer->physicalSize;

                if ((shaderContainer->flags & 0xFFFFFF00) == 0x102A1100 &&
                    dataSize > 0 && dataSize <= (fileSize - i) &&
                    shaderContainer->field1C == 0 &&
                    shaderContainer->field20 == 0)
                {
                    XXH64_hash_t hash = XXH3_64bits(shaderContainer, dataSize);
                    auto shader = shaders.try_emplace(hash);
                    if (shader.second)
                    {
                        shader.first->second.shaderData.assign(fileData.get() + i, fileData.get() + i + dataSize);
                        shader.first->second.data = shader.first->second.shaderData.data();
                    }
                    i += dataSize;
                }
                else
                {
                    i += sizeof(uint32_t);
                }
            }
        }

        std::atomic<uint32_t> progress = 0;
        size_t totalShaders = shaders.size();

        if (totalShaders > 0) {
            fmt::println("Recompiling {} unique shaders...", totalShaders);
            for (auto& [hash, shader] : shaders)
            {
                try {
                    ShaderRecompiler recompiler;
                    recompiler.recompile(shader.data, include);

                    shader.specConstantsMask = recompiler.specConstantsMask;

                    DxcCompiler dxcCompiler;
#ifdef XENOS_RECOMP_DXIL
                    shader.dxil = dxcCompiler.compile(recompiler.out, recompiler.isPixelShader, recompiler.specConstantsMask != 0, false);
                    if (shader.dxil) {
                         assert(*(reinterpret_cast<uint32_t *>(shader.dxil->GetBufferPointer()) + 1) != 0 && "DXIL was not signed properly!");
                    }
#endif
                    IDxcBlob* spirv = dxcCompiler.compile(recompiler.out, recompiler.isPixelShader, false, true);
                    if (spirv) {
                        smolv::Encode(spirv->GetBufferPointer(), spirv->GetBufferSize(), shader.spirv, smolv::kEncodeFlagStripDebugInfo);
                        spirv->Release();
                    }
                } catch (...) {
                    fmt::println(stderr, "Warning: Failed to recompile shader with hash 0x{:X}", hash);
                }

                shader.shaderData.clear();
                shader.shaderData.shrink_to_fit();
                shader.data = nullptr;

                uint32_t currentProgress = ++progress;
                if ((currentProgress % 100) == 0 || (currentProgress == totalShaders))
                    fmt::println("Progress: {}/{} ({:.1f}%)", currentProgress, totalShaders, currentProgress / float(totalShaders) * 100.0f);
            }
        }

        fmt::println("Serializing shader cache...");
        std::ofstream outFile(output, std::ios::binary);
        if (!outFile) {
            fmt::println(stderr, "Error: Could not open output file: {}", output);
            return 1;
        }

        fmt::println(outFile, "#include \"shader_cache.h\"\nShaderCacheEntry g_shaderCacheEntries[] = {{");

        std::vector<uint8_t> dxil;
        std::vector<uint8_t> spirv;

        for (auto& [hash, shader] : shaders)
        {
            fmt::println(outFile, "\t{{ 0x{:X}, {}, {}, {}, {}, {} }},",
                hash, dxil.size(), (shader.dxil != nullptr) ? shader.dxil->GetBufferSize() : 0, spirv.size(), shader.spirv.size(), shader.specConstantsMask);

            if (shader.dxil != nullptr)
            {
                dxil.insert(dxil.end(), (uint8_t*)shader.dxil->GetBufferPointer(), (uint8_t*)shader.dxil->GetBufferPointer() + shader.dxil->GetBufferSize());
                shader.dxil->Release();
                shader.dxil = nullptr;
            }
            spirv.insert(spirv.end(), shader.spirv.begin(), shader.spirv.end());
            shader.spirv.clear();
            shader.spirv.shrink_to_fit();
        }

        fmt::println(outFile, "}};");

        int level = ZSTD_maxCLevel();

#ifdef XENOS_RECOMP_DXIL
        if (!dxil.empty()) {
            size_t deSize = dxil.size();
            std::vector<uint8_t> dxilCompressed(ZSTD_compressBound(deSize));
            size_t cSize = ZSTD_compress(dxilCompressed.data(), dxilCompressed.size(), dxil.data(), deSize, level);
            dxilCompressed.resize(cSize);
            dxil.clear(); dxil.shrink_to_fit();
            writeByteArray(outFile, "g_compressedDxilCache", dxilCompressed);
            fmt::println(outFile, "extern const size_t g_dxilCacheCompressedSize = {};\nextern const size_t g_dxilCacheDecompressedSize = {};", cSize, deSize);
        }
#endif

        if (!spirv.empty()) {
            size_t deSize = spirv.size();
            std::vector<uint8_t> spirvCompressed(ZSTD_compressBound(deSize));
            size_t cSize = ZSTD_compress(spirvCompressed.data(), spirvCompressed.size(), spirv.data(), deSize, level);
            spirvCompressed.resize(cSize);
            spirv.clear(); spirv.shrink_to_fit();
            writeByteArray(outFile, "g_compressedSpirvCache", spirvCompressed);
            fmt::println(outFile, "extern const size_t g_spirvCacheCompressedSize = {};\nextern const size_t g_spirvCacheDecompressedSize = {};", cSize, deSize);
        }

        fmt::println(outFile, "extern const size_t g_shaderCacheEntryCount = {};", shaders.size());
        outFile.close();
    }
    else
    {
        ShaderRecompiler recompiler;
        size_t fileSize;
        auto bytes = readAllBytes(input, fileSize);
        if (bytes) {
            recompiler.recompile(bytes.get(), include);
            writeAllBytes(output, recompiler.out.data(), recompiler.out.size());
        }
    }

    return 0;
}
