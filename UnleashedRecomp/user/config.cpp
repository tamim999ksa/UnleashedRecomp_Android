#include "config.h"
#include <os/logger.h>
#include <ui/game_window.h>
#include <user/paths.h>

std::vector<IConfigDef*> g_configDefinitions;

#define CONFIG_DEFINE_ENUM_TEMPLATE(type) \
    static std::unordered_map<std::string, type> g_##type##_template =

CONFIG_DEFINE_ENUM_TEMPLATE(ELanguage)
{
    { "English",  ELanguage::English },
    { "Japanese", ELanguage::Japanese },
    { "German",   ELanguage::German },
    { "French",   ELanguage::French },
    { "Spanish",  ELanguage::Spanish },
    { "Italian",  ELanguage::Italian }
};

CONFIG_DEFINE_ENUM_TEMPLATE(ETimeOfDayTransition)
{
    { "Xbox",        ETimeOfDayTransition::Xbox },
    { "PlayStation", ETimeOfDayTransition::PlayStation }
};

CONFIG_DEFINE_ENUM_TEMPLATE(ECameraRotationMode)
{
    { "Normal",  ECameraRotationMode::Normal },
    { "Reverse", ECameraRotationMode::Reverse },
};

CONFIG_DEFINE_ENUM_TEMPLATE(EControllerIcons)
{
    { "Auto",        EControllerIcons::Auto },
    { "Xbox",        EControllerIcons::Xbox },
    { "PlayStation", EControllerIcons::PlayStation }
};

#include "config_scancode.inl"

CONFIG_DEFINE_ENUM_TEMPLATE(EChannelConfiguration)
{
    { "Stereo",   EChannelConfiguration::Stereo },
    { "Surround", EChannelConfiguration::Surround }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EVoiceLanguage)
{
    { "English",  EVoiceLanguage::English },
    { "Japanese", EVoiceLanguage::Japanese }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EGraphicsAPI)
{
    { "Auto", EGraphicsAPI::Auto },
#ifdef UNLEASHED_RECOMP_D3D12
    { "D3D12",  EGraphicsAPI::D3D12 },
#endif
    { "Vulkan", EGraphicsAPI::Vulkan }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EWindowState)
{
    { "Normal",    EWindowState::Normal },
    { "Maximised", EWindowState::Maximised },
    { "Maximized", EWindowState::Maximised }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EAspectRatio)
{
    { "Auto", EAspectRatio::Auto },
    { "16:9", EAspectRatio::Wide },
    { "4:3",  EAspectRatio::Narrow },
    { "Original 4:3",  EAspectRatio::OriginalNarrow },
};

CONFIG_DEFINE_ENUM_TEMPLATE(ETripleBuffering)
{
    { "Auto", ETripleBuffering::Auto },
    { "On",   ETripleBuffering::On },
    { "Off",  ETripleBuffering::Off }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EAntiAliasing)
{
    { "None",    EAntiAliasing::None },
    { "2x MSAA", EAntiAliasing::MSAA2x },
    { "4x MSAA", EAntiAliasing::MSAA4x },
    { "8x MSAA", EAntiAliasing::MSAA8x }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EShadowResolution)
{
    { "Original", EShadowResolution::Original },
    { "512",      EShadowResolution::x512 },
    { "1024",     EShadowResolution::x1024 },
    { "2048",     EShadowResolution::x2048 },
    { "4096",     EShadowResolution::x4096 },
    { "8192",     EShadowResolution::x8192 },
};

CONFIG_DEFINE_ENUM_TEMPLATE(EGITextureFiltering)
{
    { "Bilinear", EGITextureFiltering::Bilinear },
    { "Bicubic",  EGITextureFiltering::Bicubic }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EDepthOfFieldQuality)
{
    { "Auto",   EDepthOfFieldQuality::Auto },
    { "Low",    EDepthOfFieldQuality::Low },
    { "Medium", EDepthOfFieldQuality::Medium },
    { "High",   EDepthOfFieldQuality::High },
    { "Ultra",  EDepthOfFieldQuality::Ultra }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EMotionBlur)
{
    { "Off",      EMotionBlur::Off },
    { "Original", EMotionBlur::Original },
    { "Enhanced", EMotionBlur::Enhanced }
};

CONFIG_DEFINE_ENUM_TEMPLATE(ECutsceneAspectRatio)
{
    { "Original", ECutsceneAspectRatio::Original },
    { "Unlocked", ECutsceneAspectRatio::Unlocked }
};

CONFIG_DEFINE_ENUM_TEMPLATE(EUIAlignmentMode)
{
    { "Edge",    EUIAlignmentMode::Edge },
    { "Centre",  EUIAlignmentMode::Centre },
    { "Center",  EUIAlignmentMode::Centre }
};

#undef  CONFIG_DEFINE
#define CONFIG_DEFINE(section, type, name, defaultValue) \
    ConfigDef<type> Config::name{section, #name, defaultValue};

#undef  CONFIG_DEFINE_HIDDEN
#define CONFIG_DEFINE_HIDDEN(section, type, name, defaultValue) \
    ConfigDef<type, true> Config::name{section, #name, defaultValue};

#undef  CONFIG_DEFINE_LOCALISED
#define CONFIG_DEFINE_LOCALISED(section, type, name, defaultValue) \
    extern CONFIG_LOCALE g_##name##_locale; \
    ConfigDef<type> Config::name{section, #name, &g_##name##_locale, defaultValue};

#undef  CONFIG_DEFINE_ENUM
#define CONFIG_DEFINE_ENUM(section, type, name, defaultValue) \
    ConfigDef<type> Config::name{section, #name, defaultValue, &g_##type##_template};

#undef  CONFIG_DEFINE_ENUM_LOCALISED
#define CONFIG_DEFINE_ENUM_LOCALISED(section, type, name, defaultValue) \
    extern CONFIG_LOCALE g_##name##_locale; \
    extern CONFIG_ENUM_LOCALE(type) g_##type##_locale; \
    ConfigDef<type> Config::name{section, #name, &g_##name##_locale, defaultValue, &g_##type##_template, &g_##type##_locale};

#include "config_def.h"

std::filesystem::path Config::GetConfigPath()
{
    return GetUserPath() / "config.toml";
}

void Config::CreateCallbacks()
{
    Config::WindowSize.LockCallback = [](ConfigDef<int32_t>* def)
    {
        // Try matching the current window size with a known configuration.
        if (def->Value < 0)
            def->Value = GameWindow::FindNearestDisplayMode();
    };

    Config::WindowSize.ApplyCallback = [](ConfigDef<int32_t>* def)
    {
        auto displayModes = GameWindow::GetDisplayModes();

        // Use largest supported resolution if overflowed.
        if (def->Value >= displayModes.size())
            def->Value = displayModes.size() - 1;

        auto& mode = displayModes[def->Value];
        auto centre = SDL_WINDOWPOS_CENTERED_DISPLAY(GameWindow::GetDisplay());

        GameWindow::SetDimensions(mode.w, mode.h, centre, centre);
    };

    Config::Monitor.Callback = [](ConfigDef<int32_t>* def)
    {
        GameWindow::SetDisplay(def->Value);
    };

    Config::Fullscreen.Callback = [](ConfigDef<bool>* def)
    {
        GameWindow::SetFullscreen(def->Value);
        GameWindow::SetDisplay(Config::Monitor);
    };

    Config::ResolutionScale.Callback = [](ConfigDef<float>* def)
    {
        def->Value = std::clamp(def->Value, 0.25f, 2.0f);
    };
}

void Config::Load()
{
    if (!s_isCallbacksCreated)
    {
        CreateCallbacks();
        s_isCallbacksCreated = true;
    }

    auto configPath = GetConfigPath();

    if (!std::filesystem::exists(configPath))
    {
        Config::Save();
        return;
    }

    try
    {
        toml::parse_result toml;
        std::ifstream tomlStream(configPath);

        if (tomlStream.is_open())
            toml = toml::parse(tomlStream);

        for (auto def : g_configDefinitions)
        {
            def->ReadValue(toml);

#if _DEBUG
            LOGFN_UTILITY("{} (0x{:X})", def->GetDefinition().c_str(), (intptr_t)def->GetValue());
#endif
        }
    }
    catch (toml::parse_error& err)
    {
        LOGFN_ERROR("Failed to parse configuration: {}", err.what());
    }
}

void Config::Save()
{
    LOGN("Saving configuration...");

    auto userPath = GetUserPath();

    if (!std::filesystem::exists(userPath))
        std::filesystem::create_directory(userPath);

    std::string result;
    std::string section;

    for (auto def : g_configDefinitions)
    {
        if (def->IsHidden())
            continue;

        auto isFirstSection = section.empty();
        auto isDefWithSection = section != def->GetSection();
        auto tomlDef = def->GetDefinition(isDefWithSection);

        section = def->GetSection();

        // Don't output prefix space for first section.
        if (!isFirstSection && isDefWithSection)
            result += '\n';

        result += tomlDef + '\n';
    }

    std::ofstream out(GetConfigPath());

    if (out.is_open())
    {
        out << result;
        out.close();
    }
    else
    {
        LOGN_ERROR("Failed to write configuration.");
    }
}
