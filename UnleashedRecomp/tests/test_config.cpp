#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Dependencies for ConfigDef
#include <cassert>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <tuple>
#include <cstdint>
#include <iostream>
#include <functional>
#include <fmt/core.h>
#include <toml++/toml.hpp>
#include <filesystem>
#include <SDL.h>

// Mock Localise
// This needs to match the signature in locale/locale.h
std::string g_lastLocaliseKey;
std::string& Localise(const std::string_view& key) {
    static std::string result;
    g_lastLocaliseKey = key;
    result = std::string(key) + "_LOCALISED";
    return result;
}

// Global variable for locale map (declared extern in locale.h)
// We define it here to satisfy linker if it's referenced, though ConfigDef doesn't seem to use it directly.
#include <locale/locale.h>
std::unordered_map<std::string_view, std::unordered_map<ELanguage, std::string>> g_locale;

#include <user/config.h>

// Definition of global variable declared in config.h
std::vector<IConfigDef*> g_configDefinitions;

TEST_CASE("ConfigDef Basic Types") {
    g_configDefinitions.clear();

    SUBCASE("Integer Config") {
        ConfigDef<int> intConfig("TestSection", "IntKey", 42);

        CHECK(intConfig.GetSection() == "TestSection");
        CHECK(intConfig.GetName() == "IntKey");
        CHECK(intConfig.DefaultValue == 42);
        CHECK(intConfig.Value == 42);
        CHECK(intConfig.IsDefaultValue());

        intConfig = 100;
        CHECK(intConfig.Value == 100);
        CHECK(!intConfig.IsDefaultValue());

        intConfig.MakeDefault();
        CHECK(intConfig.Value == 42);
        CHECK(intConfig.IsDefaultValue());

        CHECK(g_configDefinitions.size() == 1);
        CHECK(g_configDefinitions[0] == &intConfig);

        CHECK(intConfig.ToString() == "42");
    }

    SUBCASE("String Config") {
        ConfigDef<std::string> strConfig("TestSection", "StrKey", "Default");

        CHECK(strConfig.Value == "Default");

        strConfig = "NewValue";
        CHECK(strConfig.Value == "NewValue");

        CHECK(strConfig.ToString(false) == "NewValue");
        CHECK(strConfig.ToString(true) == "\"NewValue\"");
    }

    SUBCASE("Bool Config") {
        ConfigDef<bool> boolConfig("TestSection", "BoolKey", false);

        boolConfig = true;
        CHECK(boolConfig.Value == true);

        // Bool ToString/GetValueLocalised uses Localise
        // Localise("Common_On") -> "Common_On_LOCALISED"
        CHECK(boolConfig.GetValueLocalised(ELanguage::English) == "Common_On_LOCALISED");

        boolConfig = false;
        CHECK(boolConfig.GetValueLocalised(ELanguage::English) == "Common_Off_LOCALISED");
    }
}

TEST_CASE("ConfigDef Localised") {
    g_configDefinitions.clear();

    CONFIG_LOCALE localeMap;
    localeMap[ELanguage::English] = { "LocName", "LocDesc" };
    localeMap[ELanguage::Japanese] = { "LocNameJP", "LocDescJP" };

    ConfigDef<bool> boolConfig("Section", "BoolKey", &localeMap, true);

    CHECK(boolConfig.GetNameLocalised(ELanguage::English) == "LocName");
    CHECK(boolConfig.GetDescription(ELanguage::English) == "LocDesc");
    CHECK(boolConfig.GetNameLocalised(ELanguage::Japanese) == "LocNameJP");
    CHECK(boolConfig.GetDescription(ELanguage::Japanese) == "LocDescJP");

    // Fallback to English if language not found
    CHECK(boolConfig.GetNameLocalised(ELanguage::French) == "LocName");

    // GetLocaleStrings
    std::vector<std::string_view> strings;
    boolConfig.GetLocaleStrings(strings);
    CHECK(strings.size() == 4); // 2 languages * 2 strings
}

TEST_CASE("ConfigDef Enum") {
    g_configDefinitions.clear();

    enum class TestEnum { A, B, C };
    std::unordered_map<std::string, TestEnum> enumTemplate = {
        { "OptionA", TestEnum::A },
        { "OptionB", TestEnum::B },
        { "OptionC", TestEnum::C }
    };

    ConfigDef<TestEnum> enumConfig("EnumSec", "EnumKey", TestEnum::A, &enumTemplate);

    CHECK(enumConfig.Value == TestEnum::A);
    CHECK(enumConfig.ToString(false) == "OptionA");

    enumConfig = TestEnum::B;
    CHECK(enumConfig.ToString(false) == "OptionB");

    // Test ReadValue
    auto tomlStr = R"(
        [EnumSec]
        EnumKey = "OptionC"
    )";
    auto tbl = toml::parse(tomlStr);

    enumConfig.ReadValue(tbl);
    CHECK(enumConfig.Value == TestEnum::C);
    CHECK(enumConfig.IsLoadedFromConfig);

    // Test Invalid Value in TOML
    auto tomlStrInvalid = R"(
        [EnumSec]
        EnumKey = "InvalidOption"
    )";
    auto tblInvalid = toml::parse(tomlStrInvalid);

    // If invalid value is read, it should fall back to DefaultValue (TestEnum::A)
    // Current value is C. Default is A.
    enumConfig.ReadValue(tblInvalid);
    CHECK(enumConfig.Value == TestEnum::A);
}

TEST_CASE("ConfigDef ReadValue") {
    g_configDefinitions.clear();

    ConfigDef<int> intConfig("Section", "Key", 10);

    auto tomlStr = R"(
        [Section]
        Key = 50
    )";
    auto tbl = toml::parse(tomlStr);

    CHECK(!intConfig.IsLoadedFromConfig);
    intConfig.ReadValue(tbl);
    CHECK(intConfig.Value == 50);
    CHECK(intConfig.IsLoadedFromConfig);

    // Test missing section
    auto tomlStrMissing = R"(
        [OtherSection]
        Key = 99
    )";
    auto tblMissing = toml::parse(tomlStrMissing);

    intConfig = 50;
    intConfig.IsLoadedFromConfig = false;
    intConfig.ReadValue(tblMissing);
    CHECK(intConfig.Value == 50); // Should not change
    CHECK(!intConfig.IsLoadedFromConfig); // Not loaded
}

TEST_CASE("ConfigDef GetDefinition") {
    g_configDefinitions.clear();
    ConfigDef<int> intConfig("Section", "Key", 123);

    CHECK(intConfig.GetDefinition(false) == "Key = 123");
    CHECK(intConfig.GetDefinition(true) == "[Section]\nKey = 123");
}
