# Interactive (windowed) tool to explore visual-test results: golden / actual /
# diff side by side, with golden-update controls. Not part of the headless CI
# lane. The test-helpers lib provides DirectTexture PNG I/O + comparison; pull it
# in if the test tree didn't already (this can be built without NOTHOFAGUS_BUILD_TESTS).
if(NOT TARGET nothofagus_test_helpers)
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/nothofagus_test_helpers" "${CMAKE_BINARY_DIR}/tests/nothofagus_test_helpers")
endif()

add_executable(golden_viewer
    "visual_tests_explorer.cpp"
)
set_property(TARGET golden_viewer PROPERTY CXX_STANDARD 20)
target_link_libraries(golden_viewer PRIVATE nothofagus nothofagus_test_helpers)

# Default golden / actual directories point at the visual-test source tree so the
# viewer "just works" out of the box; both are overridable at runtime (argv / env
# / the in-app directory picker).
target_compile_definitions(golden_viewer PRIVATE
    GOLDEN_VIEWER_DEFAULT_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/visual/golden"
    GOLDEN_VIEWER_DEFAULT_ACTUAL_DIR="${CMAKE_SOURCE_DIR}/tests/visual/actual"
)
