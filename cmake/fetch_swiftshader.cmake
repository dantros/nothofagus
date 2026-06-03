# Downloads a prebuilt SwiftShader Vulkan ICD at configure time and generates an
# ICD JSON descriptor pointing at it. No git submodule / subtree — the prebuilt
# is fetched on demand and verified by SHA256.
#
# Usage (from the top-level CMakeLists, at directory scope so the PARENT_SCOPE
# outputs reach sibling subdirectories like tests/):
#
#   include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_swiftshader.cmake")
#   nothofagus_fetch_swiftshader(OUT_LIB_VAR OUT_ICD_VAR)
#
# On success OUT_LIB_VAR holds the path to the downloaded .so and OUT_ICD_VAR the
# path to the generated ICD JSON. On an unsupported platform both are set empty
# and a warning is emitted (so the rest of the build proceeds without it).

include_guard(GLOBAL)

# Captured at include time — inside a function CMAKE_CURRENT_LIST_DIR resolves to
# the caller's list file, not this module's directory.
set(_nothofagus_swiftshader_module_dir "${CMAKE_CURRENT_LIST_DIR}")

function(nothofagus_fetch_swiftshader OUT_LIB_VAR OUT_ICD_VAR)
    include("${_nothofagus_swiftshader_module_dir}/swiftshader_version.cmake")

    # Only a linux-x86_64 prebuilt is published upstream today.
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux"
            AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"))
        message(WARNING
            "NOTHOFAGUS_FETCH_SWIFTSHADER: no prebuilt SwiftShader ICD for "
            "${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}; the swiftshader "
            "render backend will be unavailable on this platform.")
        set(${OUT_LIB_VAR} "" PARENT_SCOPE)
        set(${OUT_ICD_VAR} "" PARENT_SCOPE)
        return()
    endif()

    set(_dir "${CMAKE_BINARY_DIR}/swiftshader")
    set(_lib "${_dir}/libvk_swiftshader.so")
    file(MAKE_DIRECTORY "${_dir}")

    # Idempotent: file(DOWNLOAD) with EXPECTED_HASH skips the transfer when the
    # file is already present and matches, and hard-fails on a corrupt download.
    message(STATUS "Fetching SwiftShader ICD: ${NOTHOFAGUS_SWIFTSHADER_URL}")
    file(DOWNLOAD
        "${NOTHOFAGUS_SWIFTSHADER_URL}" "${_lib}"
        EXPECTED_HASH SHA256=${NOTHOFAGUS_SWIFTSHADER_SHA256}
        SHOW_PROGRESS
        STATUS _status)
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _status 1 _msg)
        message(FATAL_ERROR "SwiftShader download failed (${_code}): ${_msg}")
    endif()

    # Generate the ICD JSON pointing at the absolute downloaded .so.
    set(_icd "${_dir}/swiftshader_icd.json")
    set(NOTHOFAGUS_SWIFTSHADER_LIB_ABS "${_lib}")
    configure_file(
        "${_nothofagus_swiftshader_module_dir}/swiftshader_icd.json.in"
        "${_icd}" @ONLY)

    message(STATUS "SwiftShader ICD ready: ${_icd}")
    set(${OUT_LIB_VAR} "${_lib}" PARENT_SCOPE)
    set(${OUT_ICD_VAR} "${_icd}" PARENT_SCOPE)
endfunction()
