project("fshasher")

add_executable(fshasher "fshasher.cpp")
target_include_directories(fshasher PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/picosha2")
