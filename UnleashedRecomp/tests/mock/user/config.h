#pragma once
#include <string>
#include <vector>

// Original content for existing tests
struct Config
{
    static inline bool AchievementNotifications = true;
};

// Added for mod_loader.cpp
struct ConfigDefinition
{
    bool IsHidden() { return false; }
    std::string GetSection() { return ""; }
    std::string GetName() { return ""; }
    void* GetValue() { return nullptr; }
};

extern std::vector<ConfigDefinition*> g_configDefinitions;
