#pragma once

#include <user/achievement_data.h>
#include <unordered_map>

enum class EAchBinStatus
{
    Success,
    IOError,
    BadFileSize,
    BadSignature,
    BadVersion,
    BadChecksum
};

class AchievementManager
{
public:
    static inline AchievementData Data{};
    static inline std::unordered_map<uint16_t, time_t> s_unlockedTimestamps{};
    static inline EAchBinStatus BinStatus{ EAchBinStatus::Success };

    static std::filesystem::path GetDataPath(bool checkForMods)
    {
        return GetSavePath(checkForMods) / ACH_FILENAME;
    }

    static time_t GetTimestamp(uint16_t id);
    static size_t GetTotalRecords();
    static bool IsUnlocked(uint16_t id);
    static void Unlock(uint16_t id);
    static void UnlockAll();
    static void Reset();
    static bool LoadBinary();
    static bool SaveBinary(bool ignoreStatus = false);
};
