#pragma once

#include <stdafx.h>
#include <hid/hid.h>

class TouchControls
{
public:
    static void Init();
    static void Draw();
    static const XAMINPUT_GAMEPAD& GetState();
    static bool IsActive();
};
