#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "ui/text_wrap_utils.h"
#include <imgui.h>
#include <imgui_internal.h>

struct MockFont : public ImFont
{
    MockFont()
    {
        FontSize = 10.0f;
        FallbackAdvanceX = 0.0f; // Zero width for unknown chars (like \u200B)
        IndexAdvanceX.resize(256);
        for(int i=0; i<256; ++i)
            IndexAdvanceX[i] = 10.0f; // Each ASCII char is 10 pixels wide
    }
};

TEST_CASE("CalcWordWrapPosition")
{
    MockFont font;
    float scale = 1.0f;

    SUBCASE("Basic wrapping")
    {
        // "A A" -> widths: 10, 10, 10. Total 30.
        // Wrap at 25.
        // Should wrap at space.
        // Returns position of start of next line.
        // "A " fits. "A" goes to next line.
        // So return should be after ' '.

        const char* text = "A A";
        const char* text_end = text + 3;

        const char* result = CalcWordWrapPosition(&font, scale, text, text_end, 25.0f);

        CHECK(result == text + 2); // Points to second 'A'
    }

    SUBCASE("Zero Width Space wrapping")
    {
        // "A\u200BA"
        // 'A' (10)
        // \u200B (0, acts as break)
        // 'A' (10)

        // UTF-8: A (1), \u200B (3), A (1)
        const char* text = "A" "\xE2\x80\x8B" "A";
        const char* text_end = text + 5;

        // Wrap width 15.
        // "A" (10) fits.
        // "\u200B" (0) fits. Line width 10.
        // "A" (10). Total 20 > 15. Break.
        // Break at \u200B.
        // Return position after \u200B.

        const char* result = CalcWordWrapPosition(&font, scale, text, text_end, 15.0f);

        CHECK(result == text + 1 + 3); // Points to second 'A'
    }

    SUBCASE("No wrapping needed")
    {
        const char* text = "A" "\xE2\x80\x8B" "A";
        const char* text_end = text + 5;

        // Width 20. Wrap width 25.
        const char* result = CalcWordWrapPosition(&font, scale, text, text_end, 25.0f);

        CHECK(result == text_end);
    }
}
