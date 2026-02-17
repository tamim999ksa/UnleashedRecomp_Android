#include "perf_android.h"

#ifdef __ANDROID__

#include <android/native_window.h>
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

static PFN_APerformanceHint_getManager pAPerformanceHint_getManager = nullptr;
static PFN_APerformanceHint_createSession pAPerformanceHint_createSession = nullptr;
static PFN_APerformanceHint_reportActualWorkDuration pAPerformanceHint_reportActualWorkDuration = nullptr;
static PFN_AThermal_getCurrentThermalStatus pAThermal_getCurrentThermalStatus = nullptr;
static PFN_AThermal_acquireManager pAThermal_acquireManager = nullptr;
static PFN_ANativeWindow_setFrameRate pANativeWindow_setFrameRate = nullptr;

static APerformanceHintManager* g_hintManager = nullptr;
static APerformanceHintSession* g_hintSession = nullptr;
static AThermalManager* g_thermalManager = nullptr;

static bool g_apisLoaded = false;

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
    }
    g_apisLoaded = true;
}

namespace perf {

static std::vector<int32_t> g_threadIds;
static std::mutex g_hintMutex;

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
    {
        std::lock_guard<std::mutex> lock(g_hintMutex);
        EnsureHintSession();
    }

    if (g_hintSession && pAPerformanceHint_reportActualWorkDuration) {
        pAPerformanceHint_reportActualWorkDuration(g_hintSession, frameTimeNs);
    }
}

void SetThreadPriority(bool isRenderThread) {
    if (isRenderThread) {
        // High priority
        setpriority(PRIO_PROCESS, 0, -10);
    } else {
        // Lower priority
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
                 // Logic to reduce GPU quality could go here
            }
        }
    }
}

}

#endif
