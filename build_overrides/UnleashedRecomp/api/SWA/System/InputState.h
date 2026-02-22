#pragma once

#include "SWA.inl"
#include "PadState.h"
#include <Hedgehog/Base/Thread/hhSynchronizedPtr.h>
#include <Hedgehog/Base/Thread/hhSynchronizedObject.h>

namespace SWA
{
    class CInputState : public Hedgehog::Base::CSynchronizedObject
    {
    public:
        static Hedgehog::Base::TSynchronizedPtr<CInputState> GetInstance();

        SPadState m_PadStates[8];
        be<uint32_t> m_CurrentPadStateIndex;

        const SPadState& GetPadState() const;
    };
}

#include "InputState.inl"
