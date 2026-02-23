#include "touch_controls.h"
#include <SDL.h>
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

static XAMINPUT_GAMEPAD g_touchPadState = {};

// Config
static float g_opacity = 0.5f;
static float g_scale = 1.0f;

void TouchControls::Init()
{
    // Load resources if any
}

static bool CheckButton(ImDrawList* drawList, const ImVec2& center, float radius, const char* label, uint16_t& buttons, uint16_t mask)
{
    bool pressed = false;
    int numFingers = SDL_GetNumTouchFingers(SDL_GetTouchDevice(0));
    for (int i = 0; i < numFingers; i++)
    {
        SDL_Finger* finger = SDL_GetTouchFinger(SDL_GetTouchDevice(0), i);
        if (!finger) continue;

        ImVec2 touchPos(finger->x * ImGui::GetIO().DisplaySize.x, finger->y * ImGui::GetIO().DisplaySize.y);
        float distSqr = (touchPos.x - center.x) * (touchPos.x - center.x) + (touchPos.y - center.y) * (touchPos.y - center.y);
        if (distSqr < radius * radius)
        {
            pressed = true;
            break;
        }
    }

    ImU32 color = pressed ? IM_COL32(200, 200, 200, 200 * g_opacity) : IM_COL32(255, 255, 255, 100 * g_opacity);
    drawList->AddCircleFilled(center, radius, color);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), IM_COL32(0, 0, 0, 255), label);

    if (pressed)
        buttons |= mask;

    return pressed;
}

static void HandleStick(ImDrawList* drawList, const ImVec2& center, float radius, int16_t& outX, int16_t& outY)
{
    int closestFinger = -1;
    float closestDist = radius * 1.5f; // Reduced capture radius to avoid overlap
    ImVec2 fingerPos;

    int numFingers = SDL_GetNumTouchFingers(SDL_GetTouchDevice(0));
    for (int i = 0; i < numFingers; i++)
    {
        SDL_Finger* finger = SDL_GetTouchFinger(SDL_GetTouchDevice(0), i);
        if (!finger) continue;

        ImVec2 pos(finger->x * ImGui::GetIO().DisplaySize.x, finger->y * ImGui::GetIO().DisplaySize.y);
        float dist = sqrtf((pos.x - center.x) * (pos.x - center.x) + (pos.y - center.y) * (pos.y - center.y));

        if (dist < closestDist)
        {
            closestDist = dist;
            closestFinger = i;
            fingerPos = pos;
        }
    }

    ImVec2 knobPos = center;
    if (closestFinger != -1)
    {
        ImVec2 delta = { fingerPos.x - center.x, fingerPos.y - center.y };
        float len = sqrtf(delta.x * delta.x + delta.y * delta.y);

        if (len > radius)
        {
            delta.x = delta.x / len * radius;
            delta.y = delta.y / len * radius;
            len = radius;
        }

        knobPos = { center.x + delta.x, center.y + delta.y };

        // Map to -32767..32767
        outX = (int16_t)((delta.x / radius) * 32767.0f);
        outY = (int16_t)((-delta.y / radius) * 32767.0f);
    }
    else
    {
        outX = 0;
        outY = 0;
    }

    drawList->AddCircleFilled(center, radius, IM_COL32(100, 100, 100, 100 * g_opacity));
    drawList->AddCircleFilled(knobPos, radius * 0.4f, IM_COL32(255, 255, 255, 200 * g_opacity));
}

void TouchControls::Draw()
{
    if (SDL_GetNumTouchFingers(SDL_GetTouchDevice(0)) > 0)
    {
        hid::g_inputDevice = hid::EInputDevice::Touch;
    }

    if (hid::g_inputDevice != hid::EInputDevice::Touch && hid::g_inputDevice != hid::EInputDevice::Unknown)
        return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    memset(&g_touchPadState, 0, sizeof(g_touchPadState));

    float unit = displaySize.y / 10.0f * g_scale;

    // Left Stick (Left bottom)
    HandleStick(drawList, ImVec2(unit * 2.5f, displaySize.y - unit * 2.5f), unit * 1.2f, g_touchPadState.sThumbLX, g_touchPadState.sThumbLY);

    // Right Stick (Right bottom inner) - Moved slightly left to create more gap
    HandleStick(drawList, ImVec2(displaySize.x - unit * 4.5f, displaySize.y - unit * 2.5f), unit * 1.0f, g_touchPadState.sThumbRX, g_touchPadState.sThumbRY);

    // Face Buttons (Right bottom outer)
    ImVec2 buttonsPos = ImVec2(displaySize.x - unit * 1.5f, displaySize.y - unit * 2.5f);
    float btnRadius = unit * 0.45f;

    CheckButton(drawList, ImVec2(buttonsPos.x, buttonsPos.y + unit), btnRadius, "A", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_A);
    CheckButton(drawList, ImVec2(buttonsPos.x + unit, buttonsPos.y), btnRadius, "B", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_B);
    CheckButton(drawList, ImVec2(buttonsPos.x - unit, buttonsPos.y), btnRadius, "X", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_X);
    CheckButton(drawList, ImVec2(buttonsPos.x, buttonsPos.y - unit), btnRadius, "Y", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_Y);

    // D-Pad (Left top area relative to stick?) No, maybe separate
    ImVec2 dpadPos = ImVec2(unit * 2.5f, displaySize.y - unit * 5.5f);
    float dpadRadius = unit * 0.35f;
    CheckButton(drawList, ImVec2(dpadPos.x, dpadPos.y - unit), dpadRadius, "U", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_DPAD_UP);
    CheckButton(drawList, ImVec2(dpadPos.x, dpadPos.y + unit), dpadRadius, "D", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_DPAD_DOWN);
    CheckButton(drawList, ImVec2(dpadPos.x - unit, dpadPos.y), dpadRadius, "L", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_DPAD_LEFT);
    CheckButton(drawList, ImVec2(dpadPos.x + unit, dpadPos.y), dpadRadius, "R", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_DPAD_RIGHT);

    // Start/Back (Top center or bottom center)
    ImVec2 centerScreen = ImVec2(displaySize.x * 0.5f, displaySize.y * 0.1f);
    CheckButton(drawList, ImVec2(centerScreen.x + unit, centerScreen.y), btnRadius * 0.8f, "Start", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_START);
    CheckButton(drawList, ImVec2(centerScreen.x - unit, centerScreen.y), btnRadius * 0.8f, "Back", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_BACK);

    // Shoulders
    CheckButton(drawList, ImVec2(unit * 1.5f, unit * 3.0f), btnRadius, "LB", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_LEFT_SHOULDER);
    CheckButton(drawList, ImVec2(displaySize.x - unit * 1.5f, unit * 3.0f), btnRadius, "RB", g_touchPadState.wButtons, XAMINPUT_GAMEPAD_RIGHT_SHOULDER);

    // Triggers
    uint16_t triggers = 0;
    if (CheckButton(drawList, ImVec2(unit * 1.5f, unit * 1.5f), btnRadius, "LT", triggers, 1)) g_touchPadState.bLeftTrigger = 255;
    if (CheckButton(drawList, ImVec2(displaySize.x - unit * 1.5f, unit * 1.5f), btnRadius, "RT", triggers, 1)) g_touchPadState.bRightTrigger = 255;
}

const XAMINPUT_GAMEPAD& TouchControls::GetState()
{
    return g_touchPadState;
}

bool TouchControls::IsActive()
{
    return hid::g_inputDevice == hid::EInputDevice::Touch || hid::g_inputDevice == hid::EInputDevice::Unknown;
}
