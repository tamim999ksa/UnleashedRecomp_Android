#pragma once

#include <imgui.h>

const char* CalcWordWrapPosition(const ImFont* font, float scale, const char* text, const char* text_end, float wrap_width);
