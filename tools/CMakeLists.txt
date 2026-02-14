if (ANDROID AND DEFINED HOST_TOOLS_DIR)
    add_executable(file_to_c IMPORTED)
    set_property(TARGET file_to_c PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/file_to_c")

    add_executable(fshasher IMPORTED)
    set_property(TARGET fshasher PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/fshasher")

    add_executable(x_decompress IMPORTED)
    set_property(TARGET x_decompress PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/x_decompress")

    add_executable(bc_diff IMPORTED)
    set_property(TARGET bc_diff PROPERTY IMPORTED_LOCATION "${HOST_TOOLS_DIR}/bc_diff")

    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp)

    # Manually add XenosRecomp thirdparty to satisfy dependencies (e.g. zstd)
    # but do NOT add XenosRecomp executable because it fails to link on Android.
    set(XENOS_RECOMP_THIRDPARTY_ROOT "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp/thirdparty")
    add_subdirectory(${XENOS_RECOMP_THIRDPARTY_ROOT})
else()
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/bc_diff)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/file_to_c)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/fshasher)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/x_decompress)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp)
endif()
