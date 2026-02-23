#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "ui/string_utils.h"

TEST_CASE("Truncate") {
    SUBCASE("Basic Truncation") {
        CHECK(Truncate("Hello World", 5, false, false) == "Hello");
    }
    SUBCASE("No Truncation") {
        CHECK(Truncate("Hello", 5, false, false) == "Hello");
        CHECK(Truncate("Hello", 10, false, false) == "Hello");
    }
    SUBCASE("With Ellipsis") {
        // "Hello World" (11) > 5. Ellipsis "..." (3).
        // input.substr(0, 5-3) = "He".
        // "He" + "..." = "He..."
        CHECK(Truncate("Hello World", 5, true, false) == "He...");
    }
    SUBCASE("Prefix Ellipsis") {
        // "Hello World" (11) > 5. Ellipsis "..." (3).
        // "..." + input.substr(0, 5-3) = "..." + "He" = "...He"
        // This is the current implementation behavior.
        CHECK(Truncate("Hello World", 5, true, true) == "...He");
    }
    SUBCASE("Edge Cases") {
        CHECK(Truncate("", 5, false, false) == "");
        CHECK(Truncate("Short", 5, false, false) == "Short");
        CHECK(Truncate("Exact", 5, false, false) == "Exact");
        CHECK(Truncate("Longer", 5, false, false) == "Longe");

        // MaxLength < Ellipsis length
        // "Hello World", 2, true. Ellipsis is 3. 2 > 3 is false.
        // It falls back to no ellipsis logic: input.substr(0, 2) = "He".
        CHECK(Truncate("Hello World", 2, true, false) == "He");
    }
}
