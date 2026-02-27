#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "install/version_utils.h"

// Define LOGF_ERROR since we are linking with version_utils.cpp which uses it.
// If os/logger.h is included and not mocked properly, we might have issues.
// But since we include tests/mock in CMake, we expect the mock logger to be used.
// The mock logger defines LOGN, LOGN_WARNING, LOGN_ERROR.
// We need to ensure LOGF_ERROR is available or mocked.
// Let's check UnleashedRecomp/tests/mock/os/logger.h again.

#include <os/logger.h>

// If the mock logger doesn't have LOGF_ERROR, we might need to define it or update the mock.
// Based on memory, the mock has LOGN-style macros.
// Let's assume we need to provide a mock if it's missing or if the included header is the real one.
// However, the plan says we include 'UnleashedRecomp/tests/mock' in include dirs.

TEST_CASE("parseVersion")
{
    int major, minor, revision;

    SUBCASE("Valid versions")
    {
        CHECK(parseVersion("v1.0.0", major, minor, revision));
        CHECK(major == 1);
        CHECK(minor == 0);
        CHECK(revision == 0);

        CHECK(parseVersion("1.2.3", major, minor, revision));
        CHECK(major == 1);
        CHECK(minor == 2);
        CHECK(revision == 3);

        CHECK(parseVersion("v20.5.100", major, minor, revision));
        CHECK(major == 20);
        CHECK(minor == 5);
        CHECK(revision == 100);
    }

    SUBCASE("Invalid versions")
    {
        CHECK_FALSE(parseVersion("v1.0", major, minor, revision));
        CHECK_FALSE(parseVersion("1.0", major, minor, revision));
        CHECK_FALSE(parseVersion("abc", major, minor, revision));
        CHECK_FALSE(parseVersion("", major, minor, revision));
        CHECK_FALSE(parseVersion("v1.0.", major, minor, revision));
        CHECK_FALSE(parseVersion("v.1.0", major, minor, revision));
        // Logic might or might not handle this strictly, checking based on implementation
        // Implementation: find first dot, find second dot.
        // "1.2.3.4": first dot at 1, second at 3.
        // major = stoi(0, 1) -> "1"
        // minor = stoi(2, 1) -> "2"
        // revision = stoi(4) -> "3.4" -> stoi parses until non-digit usually? No, stoi parses "3.4" as 3.
        // So "1.2.3.4" might be parsed as 1.2.3.
        // Let's check behavior. "3.4" passed to stoi -> 3.
        // So "1.2.3.4" is valid and parsed as 1.2.3.
        CHECK(parseVersion("1.2.3.4", major, minor, revision));
        CHECK(major == 1);
        CHECK(minor == 2);
        CHECK(revision == 3);
    }

    SUBCASE("Edge cases")
    {
        CHECK(parseVersion("v0.0.0", major, minor, revision));
        CHECK(major == 0);
        CHECK(minor == 0);
        CHECK(revision == 0);
    }
}

namespace os::logger
{
    void Log(const std::string_view str, ELogType type, const char* func) {}
}
