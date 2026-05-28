# Orchestrator for the two test groups. Each group is independent — enabling
# rendering tests pulls in Catch2 and golden-image infrastructure; enabling
# correctness tests does not.
option(NOTHOFAGUS_BUILD_TESTS_RENDERING   "Build GPU/golden-image rendering tests (pulls Catch2)" ON)
option(NOTHOFAGUS_BUILD_TESTS_CORRECTNESS "Build CPU-only correctness tests" ON)

if(NOTHOFAGUS_BUILD_TESTS_RENDERING)
    # Catch2 is only needed for the rendering tests today; keep its inclusion
    # gated here so the correctness group can build without it.
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/third_party/Catch2"
        "${CMAKE_BINARY_DIR}/third_party/Catch2"
    )
    add_subdirectory(rendering)
endif()

if(NOTHOFAGUS_BUILD_TESTS_CORRECTNESS)
    add_subdirectory(tilemap_correctness)
endif()
