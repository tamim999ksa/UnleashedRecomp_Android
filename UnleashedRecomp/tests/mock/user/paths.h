#pragma once
#include <filesystem>
inline std::filesystem::path GetSavePath(bool) { return "."; }
