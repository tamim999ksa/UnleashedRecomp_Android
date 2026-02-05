#include <os/logger.h>
#include <android/log.h>

void os::logger::Init()
{
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    android_LogPriority priority = ANDROID_LOG_INFO;
    switch (type)
    {
    case ELogType::None:
    case ELogType::Utility:
        priority = ANDROID_LOG_INFO;
        break;
    case ELogType::Warning:
        priority = ANDROID_LOG_WARN;
        break;
    case ELogType::Error:
        priority = ANDROID_LOG_ERROR;
        break;
    }

    if (func && *func != '*')
    {
        __android_log_print(priority, "UnleashedRecomp", "[%s] %.*s", func, (int)str.length(), str.data());
    }
    else
    {
        __android_log_print(priority, "UnleashedRecomp", "%.*s", (int)str.length(), str.data());
    }
}
