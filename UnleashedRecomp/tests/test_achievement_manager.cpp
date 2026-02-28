#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <vector>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <map>

// Mocks
bool g_overlayOpened = false;
int g_lastOverlayId = 0;

#include <ui/achievement_overlay.h>
void AchievementOverlay::Open(int id)
{
    g_overlayOpened = true;
    g_lastOverlayId = id;
}

#include <kernel/memory_mock.h>
Memory g_memory;
bool mock_bools[4];

#include <user/achievement_manager.h>

TEST_CASE("AchievementManager")
{
    // Setup memory mappings for Reset()
    g_memory.mapped_addresses[0x833647C5] = &mock_bools[0];
    g_memory.mapped_addresses[0x83363004] = &mock_bools[1];
    g_memory.mapped_addresses[0x833647BC] = &mock_bools[2];
    g_memory.mapped_addresses[0x833647C4] = &mock_bools[3];

    // Set them to true to verify Reset clears them
    for(int i=0; i<4; ++i) mock_bools[i] = true;

    AchievementManager::Reset();
    g_overlayOpened = false;
    g_lastOverlayId = 0;

    SUBCASE("GetTimestamp")
    {
        CHECK(AchievementManager::GetTimestamp(1) == 0);

        AchievementManager::Data.Records[0].ID = 100;
        AchievementManager::Data.Records[0].Timestamp = 123456789;
        AchievementManager::s_unlockedTimestamps[100] = 123456789;

        CHECK(AchievementManager::GetTimestamp(100) == 123456789);
        CHECK(AchievementManager::GetTimestamp(999) == 0);
    }

    SUBCASE("IsUnlocked")
    {
        // Test empty state
        CHECK_FALSE(AchievementManager::IsUnlocked(200));

        // Test finding at the first position
        AchievementManager::Data.Records[0].ID = 200;
        AchievementManager::s_unlockedTimestamps[200] = 1;
        CHECK(AchievementManager::IsUnlocked(200));
        CHECK_FALSE(AchievementManager::IsUnlocked(201));

        // Test finding in the middle
        AchievementManager::Data.Records[1].ID = 300;
        AchievementManager::Data.Records[2].ID = 400;
        CHECK(AchievementManager::IsUnlocked(300));
        CHECK(AchievementManager::IsUnlocked(400));
        CHECK_FALSE(AchievementManager::IsUnlocked(500));

        // Fill up the array completely to test loop termination without hitting 0
        for (int i = 0; i < 50; i++) {
            AchievementManager::Data.Records[i].ID = 1000 + i;
        }
        // It should find the last element
        CHECK(AchievementManager::IsUnlocked(1049));
        // It should properly return false when not found in a completely full array
        CHECK_FALSE(AchievementManager::IsUnlocked(9999));

        // Reset the data so it doesn't leak into other tests
        AchievementManager::Reset();
    }

    SUBCASE("Unlock")
    {
        AchievementManager::Unlock(300);

        CHECK(AchievementManager::IsUnlocked(300));
        CHECK(g_overlayOpened);
        CHECK(g_lastOverlayId == 300);
        CHECK(AchievementManager::GetTimestamp(300) != 0);

        g_overlayOpened = false;
        AchievementManager::Unlock(300);
        CHECK_FALSE(g_overlayOpened);
    }

    SUBCASE("UnlockAll")
    {
        AchievementManager::UnlockAll();
        CHECK(AchievementManager::IsUnlocked(24));
        CHECK(AchievementManager::IsUnlocked(83));
        CHECK(AchievementManager::IsUnlocked(31));
        CHECK(AchievementManager::IsUnlocked(64));
    }

    SUBCASE("Reset")
    {
        AchievementManager::Unlock(400);
        CHECK(AchievementManager::IsUnlocked(400));

        for(int i=0; i<4; ++i) mock_bools[i] = true;

        AchievementManager::Reset();
        CHECK_FALSE(AchievementManager::IsUnlocked(400));

        for(int i=0; i<4; ++i) CHECK(mock_bools[i] == false);
    }
}
