# Orchestrator for the two test groups. Each group is independent — enabling
# the visual group pulls in Catch2 and golden-image infrastructure; the
# nonvisual group builds standalone (no third-party deps).
option(NOTHOFAGUS_BUILD_TESTS_VISUAL    "Build visual tests (pixel-level golden-image comparison; pulls Catch2)" ON)
option(NOTHOFAGUS_BUILD_TESTS_NONVISUAL "Build nonvisual tests (CPU-only data/logic checks)" ON)

if(NOTHOFAGUS_BUILD_TESTS_VISUAL)
    # Catch2 is only needed for the visual tests today; keep its inclusion
    # gated here so the nonvisual group can build without it.
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/third_party/Catch2"
        "${CMAKE_BINARY_DIR}/third_party/Catch2"
    )
    add_subdirectory(visual)
endif()

if(NOTHOFAGUS_BUILD_TESTS_NONVISUAL)
    add_subdirectory(nonvisual)
endif()
