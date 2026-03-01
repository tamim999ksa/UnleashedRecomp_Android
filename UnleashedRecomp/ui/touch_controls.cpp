#include "touch_controls.h"
#include <SDL.h>
#include <imgui.h>
#include <mutex>
#include <user/config.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

static XAMINPUT_GAMEPAD g_touchPadState = {};
static std::mutex g_touchMutex;

// Config


void TouchControls::Init()
{
    // Load resources if any
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

    XAMINPUT_GAMEPAD localState = {};

    float opacity = Config::TouchOpacity;
    float scale = Config::TouchScale;
    float unit = displaySize.y / 10.0f * scale;

    int numFingers = SDL_GetNumTouchFingers(SDL_GetTouchDevice(0));
    std::vector<ImVec2> touches;
    touches.reserve(numFingers);
    for (int i = 0; i < numFingers; i++)
    {
        SDL_Finger* finger = SDL_GetTouchFinger(SDL_GetTouchDevice(0), i);
        if (finger)
            touches.push_back(ImVec2(finger->x * displaySize.x, finger->y * displaySize.y));
    }

    auto checkButton = [&](const ImVec2& center, float radius, const char* label, uint16_t mask) {
        bool pressed = false;
        for (const auto& touch : touches)
        {
            float dx = touch.x - center.x;
            float dy = touch.y - center.y;
            if (dx*dx + dy*dy < radius * radius) {
                pressed = true;
                break;
            }
        }
        ImU32 color = pressed ? IM_COL32(200, 200, 200, 200 * opacity) : IM_COL32(255, 255, 255, 100 * opacity);
        drawList->AddCircleFilled(center, radius, color);
        ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), IM_COL32(0, 0, 0, 255), label);
        if (pressed) localState.wButtons |= mask;
    };

    auto handleStick = [&](const ImVec2& center, float radius, int16_t& outX, int16_t& outY) {
        ImVec2 fingerPos;
        bool found = false;
        float closestDist = radius * 1.5f;
        for (const auto& touch : touches)
        {
            float dx = touch.x - center.x;
            float dy = touch.y - center.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < closestDist) {
                closestDist = dist;
                fingerPos = touch;
                found = true;
            }
        }

        ImVec2 knobPos = center;
        if (found) {
            float dx = fingerPos.x - center.x;
            float dy = fingerPos.y - center.y;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > radius) {
                dx = dx / len * radius;
                dy = dy / len * radius;
                len = radius;
            }
            knobPos = ImVec2(center.x + dx, center.y + dy);
            outX = (int16_t)((dx / radius) * 32767.0f);
            outY = (int16_t)((-dy / radius) * 32767.0f);
        }
        drawList->AddCircleFilled(center, radius, IM_COL32(100, 100, 100, 100 * opacity));
        drawList->AddCircleFilled(knobPos, radius * 0.4f, IM_COL32(255, 255, 255, 200 * opacity));
    };

    // Sticks
    handleStick(ImVec2(unit * 2.5f, displaySize.y - unit * 2.5f), unit * 1.2f, localState.sThumbLX, localState.sThumbLY);
    handleStick(ImVec2(displaySize.x - unit * 4.5f, displaySize.y - unit * 2.5f), unit * 1.0f, localState.sThumbRX, localState.sThumbRY);

    // Face Buttons
    ImVec2 faceCenter = ImVec2(displaySize.x - unit * 1.5f, displaySize.y - unit * 2.5f);
    float btnR = unit * 0.45f;
    checkButton(ImVec2(faceCenter.x, faceCenter.y + unit), btnR, "A", XAMINPUT_GAMEPAD_A);
    checkButton(ImVec2(faceCenter.x + unit, faceCenter.y), btnR, "B", XAMINPUT_GAMEPAD_B);
    checkButton(ImVec2(faceCenter.x - unit, faceCenter.y), btnR, "X", XAMINPUT_GAMEPAD_X);
    checkButton(ImVec2(faceCenter.x, faceCenter.y - unit), btnR, "Y", XAMINPUT_GAMEPAD_Y);

    // D-Pad
    ImVec2 dpadCenter = ImVec2(unit * 2.5f, displaySize.y - unit * 5.5f);
    float dpadR = unit * 0.35f;
    checkButton(ImVec2(dpadCenter.x, dpadCenter.y - unit), dpadR, "U", XAMINPUT_GAMEPAD_DPAD_UP);
    checkButton(ImVec2(dpadCenter.x, dpadCenter.y + unit), dpadR, "D", XAMINPUT_GAMEPAD_DPAD_DOWN);
    checkButton(ImVec2(dpadCenter.x - unit, dpadCenter.y), dpadR, "L", XAMINPUT_GAMEPAD_DPAD_LEFT);
    checkButton(ImVec2(dpadCenter.x + unit, dpadCenter.y), dpadR, "R", XAMINPUT_GAMEPAD_DPAD_RIGHT);

    // Menu Buttons
    ImVec2 menuPos = ImVec2(displaySize.x * 0.5f, displaySize.y * 0.1f);
    checkButton(ImVec2(menuPos.x + unit, menuPos.y), btnR * 0.8f, "Start", XAMINPUT_GAMEPAD_START);
    checkButton(ImVec2(menuPos.x - unit, menuPos.y), btnR * 0.8f, "Back", XAMINPUT_GAMEPAD_BACK);

    // Shoulders & Triggers
    checkButton(ImVec2(unit * 1.5f, unit * 3.0f), btnR, "LB", XAMINPUT_GAMEPAD_LEFT_SHOULDER);
    checkButton(ImVec2(displaySize.x - unit * 1.5f, unit * 3.0f), btnR, "RB", XAMINPUT_GAMEPAD_RIGHT_SHOULDER);

    uint16_t triggerMask = 0;
    auto checkTrigger = [&](const ImVec2& pos, const char* label, uint8_t& outVal) {
        triggerMask = 0;
        checkButton(pos, btnR, label, 1);
        if (localState.wButtons & 1) outVal = 255;
        localState.wButtons &= ~1; // Clean up the temp mask
    };

    checkTrigger(ImVec2(unit * 1.5f, unit * 1.5f), "LT", localState.bLeftTrigger);
    checkTrigger(ImVec2(displaySize.x - unit * 1.5f, unit * 1.5f), "RT", localState.bRightTrigger);

    {
        std::lock_guard<std::mutex> lock(g_touchMutex);
        g_touchPadState = localState;
    }
}

XAMINPUT_GAMEPAD TouchControls::GetState()
{
    std::lock_guard<std::mutex> lock(g_touchMutex);
    return g_touchPadState;
}

bool TouchControls::IsActive()
{
    return hid::g_inputDevice == hid::EInputDevice::Touch || hid::g_inputDevice == hid::EInputDevice::Unknown;
}
