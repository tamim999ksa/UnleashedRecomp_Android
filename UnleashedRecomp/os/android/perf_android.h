#pragma once

#ifdef __ANDROID__
#include <cstdint>
#include <SDL.h>

namespace perf {
    void EnableSustainedPerformanceMode(bool enable);
    void RegisterHintThread(int32_t tid);
    void ReportFrameTime(int64_t frameTimeNs);
    void SetThreadPriority(bool isRenderThread);
    void ConfigureNativeWindow(SDL_Window* window);
    void MonitorThermals();
    void InitChoreographer();
}
#endif
    void SetBigCoreAffinity();
