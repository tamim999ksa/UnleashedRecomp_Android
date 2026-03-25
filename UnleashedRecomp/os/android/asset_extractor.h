#pragma once

#ifdef __ANDROID__

#include <filesystem>
#include <functional>

// Extracts embedded game data from APK assets to the game directory.
// Returns true if extraction succeeded or no embedded data is present.
// The progressCallback receives (bytesExtracted, totalBytes) and should return
// false to cancel extraction.
bool ExtractEmbeddedGameData(
    const std::filesystem::path& targetPath,
    const std::function<bool(uint64_t, uint64_t)>& progressCallback = nullptr);

// Returns true if the APK contains embedded game data.
bool HasEmbeddedGameData();

#endif
