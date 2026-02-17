#pragma once

#ifdef __ANDROID__

#include <cstdint>

struct SDL_Window;

namespace perf {
    void EnableSustainedPerformanceMode(bool enable);
    void ReportFrameTime(int64_t frameTimeNs);
    void SetThreadPriority(bool isRenderThread);
    void RegisterHintThread(int32_t tid);
    void ConfigureNativeWindow(SDL_Window* window);
    void MonitorThermals();
}

#endif
