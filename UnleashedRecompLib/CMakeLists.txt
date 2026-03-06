project("UnleashedRecompLib")

set(MISSING_ASSETS "")
if(NOT BUILD_TOOLS_ONLY)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/default.xex")
        list(APPEND MISSING_ASSETS "default.xex")
    endif()

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/shader.ar")
        list(APPEND MISSING_ASSETS "shader.ar")
    endif()

    if(MISSING_ASSETS)
        message(FATAL_ERROR "\n\n[UnleashedRecomp] ERROR: Missing original game assets in UnleashedRecompLib/private/:\n"
                            "  ${MISSING_ASSETS}\n\n"
                            "To fix this:\n"
                            "1. Local build: Place the original game files in UnleashedRecompLib/private/\n"
                            "2. GitHub Actions: Ensure \"game_url\" and \"dlc_url\" inputs are provided correctly.\n\n")
    endif()
endif()

add_compile_options(
    -fno-strict-aliasing
)

if (WIN32)
    add_compile_options(/fp:strict)
elseif(NOT ANDROID)
    add_compile_options(-ffp-model=strict)
endif()

set(UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp_shared.h"
)

foreach(i RANGE 0 339)
    list(APPEND UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp.${i}.cpp")
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ppc")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shader")


# Configure optional dependency for patch file
set(DEFAULT_XEXP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/private/default.xexp")
if(EXISTS "${DEFAULT_XEXP_PATH}")
    set(DEFAULT_XEXP_DEP "${DEFAULT_XEXP_PATH}")
else()
    set(DEFAULT_XEXP_DEP "")
endif()

add_custom_command(

    OUTPUT 
        "${CMAKE_CURRENT_SOURCE_DIR}/private/default_patched.xex"
        ${UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/ppc"
    COMMAND $<TARGET_FILE:XenonRecompHost> "${CMAKE_CURRENT_SOURCE_DIR}/config/SWA.toml" "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp/XenonUtils/ppc_context.h"
    DEPENDS 
        "${CMAKE_CURRENT_SOURCE_DIR}/private/default.xex"
        ${DEFAULT_XEXP_DEP}
        "${CMAKE_CURRENT_SOURCE_DIR}/config/SWA.toml"
    USES_TERMINAL
)

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_decompressed.ar"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/private"
    COMMAND $<TARGET_FILE:x_decompress> "${CMAKE_CURRENT_SOURCE_DIR}/private/shader.ar" "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_decompressed.ar"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/private/shader.ar"
)

set(XENOS_RECOMP_ROOT "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp/XenosRecomp")
set(XENOS_RECOMP_INCLUDE "${XENOS_RECOMP_ROOT}/shader_common.h")

file(GLOB XENOS_RECOMP_SOURCES 
    "${XENOS_RECOMP_ROOT}/*.h"
    "${XENOS_RECOMP_ROOT}/*.cpp"
)

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/shader"
    COMMAND $<TARGET_FILE:XenosRecompHost> "${CMAKE_CURRENT_SOURCE_DIR}/private" "${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp" "${XENOS_RECOMP_INCLUDE}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/private/default_patched.xex"
        "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_decompressed.ar"
        ${XENOS_RECOMP_SOURCES}
        ${XENOS_RECOMP_INCLUDE}
    USES_TERMINAL
)

add_library(UnleashedRecompLib 
    ${UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES}
    "shader/shader_cache.h"
    "shader/shader_cache.cpp"
)

target_include_directories(UnleashedRecompLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(UnleashedRecompLib PRIVATE "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp/thirdparty/simde")
target_precompile_headers(UnleashedRecompLib PUBLIC "ppc/ppc_recomp_shared.h")
