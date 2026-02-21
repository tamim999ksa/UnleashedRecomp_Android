#include "text_wrap_utils.h"
#include <imgui_internal.h>

// Helper to determine if a character allows wrapping.
// Modified from ImCharIsBlankW to include 0x200B (Zero Width Space) support.
static bool IsCharWrappable(unsigned int c)
{
    return ImCharIsBlankW(c) || c == 0x200B;
}

// Taken from ImGui and modified to break for '\u200B' too.
// Simple word-wrapping for English, not full-featured. Please submit failing cases!
// This will return the next location to wrap from. If no wrapping if necessary, this will fast-forward to e.g. text_end.
const char* CalcWordWrapPosition(const ImFont* font, float scale, const char* text, const char* text_end, float wrap_width)
{
    float line_width = 0.0f;
    float word_width = 0.0f;
    wrap_width /= scale;

    const char* s = text;
    const char* last_break_pos = nullptr;

    while (s < text_end)
    {
        unsigned int c = (unsigned int)*s;
        const char* next_s;
        if (c < 0x80)
            next_s = s + 1;
        else
            next_s = s + ImTextCharFromUtf8(&c, s, text_end);

        if (c < 32)
        {
            if (c == '\n')
            {
                line_width = word_width = 0.0f;
                last_break_pos = next_s;
                s = next_s;
                continue;
            }
            if (c == '\r')
            {
                s = next_s;
                continue;
            }
        }

        const float char_width = ((int)c < font->IndexAdvanceX.Size ? font->IndexAdvanceX.Data[c] : font->FallbackAdvanceX);

        // Determine if we can break after this char
        bool can_break_after = false;
        bool is_space = IsCharWrappable(c);

        // Lookahead
        unsigned int next_c = 0;
        if (next_s < text_end)
        {
            next_c = (unsigned int)*next_s;
            if (next_c >= 0x80)
            {
                // We don't really need full UTF8 decoding for next_c just for basic punctuation checks unless it's multi-byte punctuation.
                // But for safety and correctness if needed:
                ImTextCharFromUtf8(&next_c, next_s, text_end);
            }
        }

        if (is_space)
        {
            // Space is a break point, UNLESS followed by sticky punctuation (French spacing rule)
            if (next_c == '!' || next_c == '?' || next_c == ':' || next_c == ';')
                can_break_after = false;
            else
                can_break_after = true;
        }
        else
        {
            // Punctuation rules
            if (c == ',' || c == ';' || c == '-')
                can_break_after = true;
            else if (c == '.')
            {
                // Don't break '...' or '1.2'
                // Check if between digits
                bool prev_digit = (s > text && *(s - 1) >= '0' && *(s - 1) <= '9');
                bool next_digit = (next_c >= '0' && next_c <= '9');

                if (prev_digit && next_digit)
                    can_break_after = false;
                else if (next_c == '.') // Ellipsis
                    can_break_after = false;
                else
                    can_break_after = true;
            }
            else if (c == '!' || c == '?')
            {
                // Sticky punctuation: stick to previous word, and stick to subsequent punctuation of same type
                if (next_c == '!' || next_c == '?')
                    can_break_after = false;
                else
                    // Standard: break allowed after punctuation unless space follows (space handles the break)
                    // But if we have "Word!Word", we allow break.
                    can_break_after = true;
            }
        }

        word_width += char_width;

        if (can_break_after)
        {
            if (line_width + word_width > wrap_width)
            {
                if (last_break_pos) return last_break_pos;
                return (s == text) ? next_s : s; // Force break, ensure progress
            }

            line_width += word_width;
            word_width = 0.0f;
            last_break_pos = next_s;
        }
        else
        {
            if (line_width + word_width > wrap_width)
            {
                if (last_break_pos) return last_break_pos;
                return (s == text) ? next_s : s; // Force break, ensure progress
            }
        }

        s = next_s;
    }

    return s;
}
