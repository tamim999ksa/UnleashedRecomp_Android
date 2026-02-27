#include "mock_swa_minimal.h"

// Shadow the SWA include path by placing a fake SWA.inl in this directory
// and including it via standard include paths.
// The build system or command line must prioritize this directory.

#ifndef _HEDGEHOG_BASE_HH_OBJECT_H_
#define _HEDGEHOG_BASE_HH_OBJECT_H_
namespace Hedgehog::Base {
}
#endif

#include "Hedgehog/Universe/Engine/hhUpdateInfo.h"
#include "Hedgehog/Universe/Thread/hhParallelJob.h"
#include "Hedgehog/Universe/Engine/hhUpdateUnit.h"

#include <cstdio>

// Provide definitions for CUpdateUnit methods to satisfy linker
namespace Hedgehog::Universe {
    CUpdateUnit::CUpdateUnit() : CObject(), IParallelJob() {}
    void CUpdateUnit::ExecuteParallelJob(const SUpdateInfo& in_rUpdateInfo) {
        printf("CUpdateUnit::ExecuteParallelJob base called\n");
    }
}

class TestUpdateUnit : public Hedgehog::Universe::CUpdateUnit
{
public:
    TestUpdateUnit() : CUpdateUnit() {}

    virtual void ExecuteParallelJob(const Hedgehog::Universe::SUpdateInfo& in_rUpdateInfo) override
    {
        printf("TestUpdateUnit::ExecuteParallelJob called\n");
    }

    virtual void UpdateParallel(const Hedgehog::Universe::SUpdateInfo& in_rUpdateInfo) override
    {
        printf("TestUpdateUnit::UpdateParallel called\n");
    }

    virtual void UpdateSerial(const Hedgehog::Universe::SUpdateInfo& in_rUpdateInfo) override
    {
        printf("TestUpdateUnit::UpdateSerial called\n");
    }
};

int main()
{
    TestUpdateUnit unit;
    Hedgehog::Universe::SUpdateInfo updateInfo;
    unit.ExecuteParallelJob(updateInfo);
    unit.UpdateParallel(updateInfo);
    unit.UpdateSerial(updateInfo);

    return 0;
}
