if (ANDROID AND DEFINED HOST_TOOLS_DIR)
    add_executable(file_to_c IMPORTED GLOBAL)
    set_property(TARGET file_to_c PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/file_to_c")

    add_executable(fshasher IMPORTED GLOBAL)
    set_property(TARGET fshasher PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/fshasher")

    add_executable(x_decompress IMPORTED GLOBAL)
    set_property(TARGET x_decompress PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/x_decompress")

    add_executable(bc_diff IMPORTED GLOBAL)
    set_property(TARGET bc_diff PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/bc_diff")

    add_executable(XenonRecompHost IMPORTED GLOBAL)
    set_property(TARGET XenonRecompHost PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/XenonRecomp")

    add_executable(XenosRecompHost IMPORTED GLOBAL)
    set_property(TARGET XenosRecompHost PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/XenosRecomp")

    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp)

    # Manually add XenosRecomp thirdparty to satisfy dependencies (e.g. zstd)
    # but do NOT add XenosRecomp executable because it fails to link on Android.
    set(XENOS_RECOMP_THIRDPARTY_ROOT "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp/thirdparty")
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${XENOS_RECOMP_THIRDPARTY_ROOT})
else()
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/bc_diff)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/file_to_c)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/fshasher)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/x_decompress)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp)
    add_executable(XenonRecompHost ALIAS XenonRecomp)

    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp)
    add_executable(XenosRecompHost ALIAS XenosRecomp)

    if(TARGET XenosRecomp)
        get_target_property(XENOS_RECOMP_IMPORTED XenosRecomp IMPORTED)
        if(NOT XENOS_RECOMP_IMPORTED)
            target_compile_definitions(XenosRecomp PRIVATE UNLEASHED_RECOMP)
        endif()
    endif()
endif()
