#pragma once
#include <filesystem>

// Original content for existing tests
inline std::filesystem::path GetSavePath(bool) { return "."; }

// Added for mod_loader.cpp
inline std::filesystem::path GetGamePath() { return "."; }
inline const std::filesystem::path& GetUserPath() { static std::filesystem::path p("."); return p; }
