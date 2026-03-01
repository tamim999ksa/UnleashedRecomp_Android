#include <stdafx.h>
#include <user/paths.h>
#include <fstream>
#include <vector>

namespace vulkan_utils {

std::filesystem::path GetPipelineCachePath() {
    return GetUserPath() / "vulkan_pipeline_cache.bin";
}

std::vector<uint8_t> LoadPipelineCache() {
    auto path = GetPipelineCachePath();
    if (!std::filesystem::exists(path)) return {};

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read((char*)data.data(), size);
    return data;
}

void SavePipelineCache(const void* data, size_t size) {
    auto path = GetPipelineCachePath();
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.write((const char*)data, size);
    }
}

}
