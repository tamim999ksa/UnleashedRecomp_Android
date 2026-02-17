#pragma once

#include <cstdint>
#include <xbox.h>

namespace AudioInternal
{
    static constexpr uint32_t AUDIO_CENTER_PTR_ADDR = 0x83362FFC;

    struct CategoryVolume
    {
        uint8_t pad[0x8];
        be<float> volume;
        uint8_t pad2[0x4];
    };

    static_assert(sizeof(CategoryVolume) == 0x10, "CategoryVolume size mismatch");

    struct AudioSubsystem
    {
        uint8_t pad[0x70];
        be<float> effectsVolume;    // 0x70
        be<float> musicVolume;      // 0x74
        uint8_t pad2[0x4];          // 0x78
        CategoryVolume categories[1]; // 0x7C - variable length array
    };

    struct AudioCenter
    {
        uint8_t pad[0x4];
        be<uint32_t> subsystemPtr; // Offset 0x4
    };
}
