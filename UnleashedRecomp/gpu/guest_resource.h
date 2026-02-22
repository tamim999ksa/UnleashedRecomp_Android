#pragma once

#include <xbox.h>

enum class ResourceType
{
    Texture,
    VolumeTexture,
    VertexBuffer,
    IndexBuffer,
    RenderTarget,
    DepthStencil,
    VertexDeclaration,
    VertexShader,
    PixelShader
};

struct GuestResource
{
    uint32_t unused = 0;
    be<uint32_t> refCount = 1;
    ResourceType type;

    GuestResource(ResourceType type) : type(type)
    {
    }

    void AddRef()
    {
        uint32_t originalValue = refCount.value;
        uint32_t incrementedValue;
        do
        {
            incrementedValue = ByteSwap(ByteSwap(originalValue) + 1);
        } while (!__atomic_compare_exchange_n(&refCount.value, &originalValue, incrementedValue, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    }

    void Release()
    {
        uint32_t originalValue = refCount.value;
        uint32_t decrementedValue;
        do
        {
            decrementedValue = ByteSwap(ByteSwap(originalValue) - 1);
        } while (!__atomic_compare_exchange_n(&refCount.value, &originalValue, decrementedValue, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));

        // Normally we are supposed to release here, so only use this
        // function when you know you won't be the one destructing it.
    }
};
