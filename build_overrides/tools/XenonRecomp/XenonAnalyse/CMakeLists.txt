project("XenonAnalyse")

if (NOT ANDROID)
    add_executable(XenonAnalyse
        "main.cpp"
        "function.cpp")

    target_link_libraries(XenonAnalyse PRIVATE XenonUtils fmt::fmt)
endif()

add_library(LibXenonAnalyse "function.cpp")
target_include_directories(LibXenonAnalyse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LibXenonAnalyse PUBLIC XenonUtils)
