#pragma once

#include "SWA.inl"
#include "Hedgehog/Universe/Engine/hhUpdateInfo.h"

namespace Hedgehog::Universe
{
    class IParallelJob
    {
    public:
        xpointer<void> m_pVftable;

        IParallelJob() {}
        IParallelJob(const swa_null_ctor&) {}

        ~IParallelJob()
        {
            SWA_VIRTUAL_FUNCTION(void, 0, this);
        }

        void ExecuteParallelJob(const SUpdateInfo& in_rUpdateInfo)
        {
            SWA_VIRTUAL_FUNCTION(void, 1, this, &in_rUpdateInfo);
        }
    };
}
