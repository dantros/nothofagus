# Orchestrator for the two test groups. Both groups use Catch2, but the visual
# group additionally requires a render backend and golden-image infrastructure.
option(NOTHOFAGUS_BUILD_TESTS_VISUAL    "Build visual tests (pixel-level golden-image comparison)" OFF)
option(NOTHOFAGUS_BUILD_TESTS_NONVISUAL "Build nonvisual tests (CPU-only data/logic checks)" OFF)

# Catch2 is shared infrastructure for both groups; include it once when either is on.
if(NOTHOFAGUS_BUILD_TESTS_VISUAL OR NOTHOFAGUS_BUILD_TESTS_NONVISUAL)
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/third_party/Catch2"
        "${CMAKE_BINARY_DIR}/third_party/Catch2"
    )
endif()

if(NOTHOFAGUS_BUILD_TESTS_VISUAL)
    # Shared golden-image helper (PNG I/O + comparison). Guarded so the viewer
    # tool can also pull it in independently.
    if(NOT TARGET nothofagus_golden)
        add_subdirectory(golden)
    endif()
    add_subdirectory(visual)
endif()

if(NOTHOFAGUS_BUILD_TESTS_NONVISUAL)
    add_subdirectory(nonvisual)
endif()
