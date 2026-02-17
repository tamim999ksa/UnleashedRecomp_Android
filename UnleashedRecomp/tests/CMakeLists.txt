
add_executable(test_memory test_memory.cpp)

# Include directories
target_include_directories(test_memory PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UnleashedRecomp
)

# Set C++ standard
target_compile_features(test_memory PRIVATE cxx_std_20)

add_test(NAME MemoryTest COMMAND test_memory)
