#pragma once

namespace os::registry
{
    bool Init();

    template<typename T>
    bool ReadValue(const std::string_view& name, T& data);

    template<typename T>
    bool WriteValue(const std::string_view& name, const T& data);
}

#include <os/android/registry_android.inl>
