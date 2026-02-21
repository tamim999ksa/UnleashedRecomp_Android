add_executable(test_memory test_memory.cpp)

# Include directories
target_include_directories(test_memory PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UnleashedRecomp
)

# Set C++ standard
target_compile_features(test_memory PRIVATE cxx_std_20)

add_test(NAME MemoryTest COMMAND test_memory)

# test_config
add_executable(test_config test_config.cpp)

target_include_directories(test_config PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UnleashedRecomp
)

target_compile_features(test_config PRIVATE cxx_std_20)

target_link_libraries(test_config PRIVATE
    fmt::fmt
    tomlplusplus::tomlplusplus
    SDL2::SDL2-static
)

add_test(NAME ConfigTest COMMAND test_config)

# test_directory_file_system
add_executable(test_directory_file_system test_directory_file_system.cpp)

target_include_directories(test_directory_file_system PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UnleashedRecomp
)

target_compile_features(test_directory_file_system PRIVATE cxx_std_20)

add_test(NAME DirectoryFileSystemTest COMMAND test_directory_file_system)

# test_mutex
add_executable(test_mutex test_mutex.cpp)

target_include_directories(test_mutex PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UnleashedRecomp
)

target_compile_features(test_mutex PRIVATE cxx_std_20)

# On Linux, we need to link against pthreads implicitly, usually covered by compiler or standard lib
# But if threads are used, sometimes we need to link against Threads::Threads.
find_package(Threads REQUIRED)
target_link_libraries(test_mutex PRIVATE Threads::Threads)

add_test(NAME MutexTest COMMAND test_mutex)
