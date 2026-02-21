#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <filesystem>
#include <vector>
#include <string>
#include <os/process.h>

TEST_CASE("OS Process Utilities") {
    SUBCASE("GetExecutablePath") {
        std::filesystem::path exePath = os::process::GetExecutablePath();
        CHECK(!exePath.empty());
        CHECK(exePath.is_absolute());
        CHECK(std::filesystem::exists(exePath));

        // The test executable name should end with "test_os_process"
        CHECK(exePath.filename() == "test_os_process");
    }

    SUBCASE("GetExecutableRoot") {
        std::filesystem::path exePath = os::process::GetExecutablePath();
        std::filesystem::path exeRoot = os::process::GetExecutableRoot();

        CHECK(!exeRoot.empty());
        CHECK(exeRoot.is_absolute());
        // remove_filename() leaves a trailing slash, parent_path() removes it.
        // Use equivalent() to compare filesystem paths.
        CHECK(std::filesystem::equivalent(exePath.parent_path(), exeRoot));
    }

    SUBCASE("GetWorkingDirectory") {
        std::filesystem::path cwd = os::process::GetWorkingDirectory();
        std::filesystem::path stdCwd = std::filesystem::current_path();

        // They should be equivalent
        CHECK(std::filesystem::equivalent(cwd, stdCwd));
    }

    SUBCASE("SetWorkingDirectory") {
        std::filesystem::path originalCwd = os::process::GetWorkingDirectory();
        std::filesystem::path newCwd = originalCwd.parent_path();

        // Change to parent directory
        bool success = os::process::SetWorkingDirectory(newCwd);
        CHECK(success);

        std::filesystem::path currentCwd = os::process::GetWorkingDirectory();
        CHECK(std::filesystem::equivalent(currentCwd, newCwd));

        // Restore original CWD
        success = os::process::SetWorkingDirectory(originalCwd);
        CHECK(success);
        CHECK(std::filesystem::equivalent(os::process::GetWorkingDirectory(), originalCwd));
    }
}
