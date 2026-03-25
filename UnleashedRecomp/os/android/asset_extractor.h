#pragma once

#ifdef __ANDROID__

#include <filesystem>
#include <functional>

// Extracts embedded game data from APK assets to the game directory.
// Returns true if extraction succeeded.
// The progressCallback receives (bytesExtracted, totalBytes) and should return
// false to cancel extraction.
bool ExtractEmbeddedGameData(
    const std::filesystem::path& targetPath,
    const std::function<bool(uint64_t, uint64_t)>& progressCallback = nullptr);

// Returns true if the APK contains embedded game data.
bool HasEmbeddedGameData();

// Extracts a sideloaded game_data.tar.zst from the game directory.
// The user copies this file (produced by the workflow) to the game path.
// Returns true if extraction succeeded. The archive is deleted after extraction.
bool ExtractExternalGameData(
    const std::filesystem::path& gamePath,
    const std::function<bool(uint64_t, uint64_t)>& progressCallback = nullptr);

// Returns true if a sideloaded game_data.tar.zst exists in the game directory.
bool HasExternalGameData(const std::filesystem::path& gamePath);

#endif
