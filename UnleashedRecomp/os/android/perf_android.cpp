#include "perf_android.h"

#ifdef __ANDROID__

#include <android/native_window.h>
#include <android/choreographer.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>
#include <jni.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <mutex>
#include <vector>
#include <deque>
#include <numeric>
#include <atomic>
#include <os/logger.h>
#include <user/config.h>

// Dynamic API loading helpers
typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;
typedef struct AThermalManager AThermalManager;

typedef APerformanceHintManager* (*PFN_APerformanceHint_getManager)();
typedef APerformanceHintSession* (*PFN_APerformanceHint_createSession)(APerformanceHintManager* manager, const int32_t* threadIds, size_t size, int64_t initialTargetWorkDurationNanos);
typedef int (*PFN_APerformanceHint_reportActualWorkDuration)(APerformanceHintSession* session, int64_t actualWorkDurationNanos);
typedef int (*PFN_AThermal_getCurrentThermalStatus)(AThermalManager* manager);
typedef AThermalManager* (*PFN_AThermal_acquireManager)(void);
typedef int (*PFN_ANativeWindow_setFrameRate)(ANativeWindow* window, float frameRate, int8_t compatibility);
typedef AChoreographer* (*PFN_AChoreographer_getInstance)();
typedef void (*PFN_AChoreographer_postFrameCallback)(AChoreographer* choreographer, AChoreographer_frameCallback callback, void* data);

static PFN_APerformanceHint_getManager pAPerformanceHint_getManager = nullptr;
static PFN_APerformanceHint_createSession pAPerformanceHint_createSession = nullptr;
static PFN_APerformanceHint_reportActualWorkDuration pAPerformanceHint_reportActualWorkDuration = nullptr;
static PFN_AThermal_getCurrentThermalStatus pAThermal_getCurrentThermalStatus = nullptr;
static PFN_AThermal_acquireManager pAThermal_acquireManager = nullptr;
static PFN_ANativeWindow_setFrameRate pANativeWindow_setFrameRate = nullptr;
static PFN_AChoreographer_getInstance pAChoreographer_getInstance = nullptr;
static PFN_AChoreographer_postFrameCallback pAChoreographer_postFrameCallback = nullptr;

static APerformanceHintManager* g_hintManager = nullptr;
static APerformanceHintSession* g_hintSession = nullptr;
static AThermalManager* g_thermalManager = nullptr;
static AChoreographer* g_choreographer = nullptr;

static bool g_apisLoaded = false;
static std::atomic<long> g_lastVsyncTimeNanos{0};

static void LoadAndroidAPIs() {
    if (g_apisLoaded) return;

    void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib) {
        pAPerformanceHint_getManager = (PFN_APerformanceHint_getManager)dlsym(lib, "APerformanceHint_getManager");
        pAPerformanceHint_createSession = (PFN_APerformanceHint_createSession)dlsym(lib, "APerformanceHint_createSession");
        pAPerformanceHint_reportActualWorkDuration = (PFN_APerformanceHint_reportActualWorkDuration)dlsym(lib, "APerformanceHint_reportActualWorkDuration");
        pAThermal_getCurrentThermalStatus = (PFN_AThermal_getCurrentThermalStatus)dlsym(lib, "AThermal_getCurrentThermalStatus");
        pAThermal_acquireManager = (PFN_AThermal_acquireManager)dlsym(lib, "AThermal_acquireManager");
        pANativeWindow_setFrameRate = (PFN_ANativeWindow_setFrameRate)dlsym(lib, "ANativeWindow_setFrameRate");
        pAChoreographer_getInstance = (PFN_AChoreographer_getInstance)dlsym(lib, "AChoreographer_getInstance");
        pAChoreographer_postFrameCallback = (PFN_AChoreographer_postFrameCallback)dlsym(lib, "AChoreographer_postFrameCallback");
    }
    g_apisLoaded = true;
}

namespace perf {

static std::vector<int32_t> g_threadIds;
static std::mutex g_hintMutex;
static std::deque<int64_t> g_frameTimeHistory;
static constexpr size_t MAX_HISTORY = 8;

void EnableSustainedPerformanceMode(bool enable) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) return;

    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) return;

    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "setSustainedPerformanceMode", "(Z)V");
    if (method) {
        env->CallVoidMethod(activity, method, enable);
    }
    env->DeleteLocalRef(cls);
}

void RegisterHintThread(int32_t tid) {
    std::lock_guard<std::mutex> lock(g_hintMutex);
    g_threadIds.push_back(tid);
}

static void EnsureHintSession() {
    if (!g_apisLoaded) LoadAndroidAPIs();

    if (!g_hintSession && !g_threadIds.empty() && pAPerformanceHint_getManager && pAPerformanceHint_createSession) {
        if (!g_hintManager) g_hintManager = pAPerformanceHint_getManager();
        if (g_hintManager) {
            int64_t targetNs = 16666666;
            if (Config::FPS > 0) {
                 targetNs = 1000000000 / Config::FPS;
            }
            g_hintSession = pAPerformanceHint_createSession(g_hintManager, g_threadIds.data(), g_threadIds.size(), targetNs);
        }
    }
}

void ReportFrameTime(int64_t frameTimeNs) {
    std::lock_guard<std::mutex> lock(g_hintMutex);
    EnsureHintSession();

    if (g_hintSession && pAPerformanceHint_reportActualWorkDuration) {
        g_frameTimeHistory.push_back(frameTimeNs);
        if (g_frameTimeHistory.size() > MAX_HISTORY) {
            g_frameTimeHistory.pop_front();
        }

        int64_t smoothedTime = std::accumulate(g_frameTimeHistory.begin(), g_frameTimeHistory.end(), 0LL) / g_frameTimeHistory.size();
        pAPerformanceHint_reportActualWorkDuration(g_hintSession, smoothedTime);
    }
}

void SetThreadPriority(bool isRenderThread) {
    if (isRenderThread) {
        setpriority(PRIO_PROCESS, 0, -10);
    } else {
        setpriority(PRIO_PROCESS, 0, 0);
    }
}

void ConfigureNativeWindow(SDL_Window* window) {
    if (!window) return;

    LoadAndroidAPIs();

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_ANDROID) {
        ANativeWindow* nativeWindow = (ANativeWindow*)info.info.android.window;
        if (nativeWindow) {
             ANativeWindow_setBuffersGeometry(nativeWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);

             if (pANativeWindow_setFrameRate && Config::FPS > 0) {
                 pANativeWindow_setFrameRate(nativeWindow, (float)Config::FPS, 1 /* ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE */);
             }
        }
    }
}

void MonitorThermals() {
    static int frameCount = 0;
    if (frameCount++ < 60) return;
    frameCount = 0;

    LoadAndroidAPIs();
    if (pAThermal_acquireManager && pAThermal_getCurrentThermalStatus) {
        if (!g_thermalManager) {
            g_thermalManager = pAThermal_acquireManager();
        }

        if (g_thermalManager) {
            int status = pAThermal_getCurrentThermalStatus(g_thermalManager);
            if (status >= 3) { // ATHERMAL_STATUS_SEVERE
                 LOGFN_WARN("Severe thermal status detected!");
            }
        }
    }
}

static void ChoreographerCallback(long frameTimeNanos, void* data) {
    g_lastVsyncTimeNanos.store(frameTimeNanos, std::memory_order_release);
    if (g_choreographer && pAChoreographer_postFrameCallback) {
        pAChoreographer_postFrameCallback(g_choreographer, ChoreographerCallback, nullptr);
    }
}

void InitChoreographer() {
    LoadAndroidAPIs();
    if (pAChoreographer_getInstance && pAChoreographer_postFrameCallback) {
        g_choreographer = pAChoreographer_getInstance();
        if (g_choreographer) {
            pAChoreographer_postFrameCallback(g_choreographer, ChoreographerCallback, nullptr);
        }
    }
}

void SetBigCoreAffinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int numCores = sysconf(_SC_NPROCESSORS_CONF);
    if (numCores > 4) {
        for (int i = numCores / 2; i < numCores; i++) {
            CPU_SET(i, &cpuset);
        }
    } else {
        for (int i = 0; i < numCores; i++) {
            CPU_SET(i, &cpuset);
        }
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

}

#endif
