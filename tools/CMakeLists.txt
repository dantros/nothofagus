# Developer tools built on top of Nothofagus. Currently just the golden-image
# viewer. These are interactive (windowed) and not part of the headless CI lane.

# The test-helpers lib provides DirectTexture PNG I/O + comparison; pull it in if
# the test tree didn't already (the viewer can be built without NOTHOFAGUS_BUILD_TESTS).
if(NOT TARGET nothofagus_test_helpers)
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/golden" "${CMAKE_BINARY_DIR}/tests/golden")
endif()

add_subdirectory(golden_viewer)
