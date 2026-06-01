# Interactive (windowed) tool to explore visual-test results: golden / actual /
# diff side by side, with golden-update controls. Not part of the headless CI
# lane. The test-helpers lib provides DirectTexture PNG I/O + comparison; pull it
# in if the test tree didn't already (this can be built without NOTHOFAGUS_BUILD_TESTS).
if(NOT TARGET nothofagus_test_helpers)
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/nothofagus_test_helpers" "${CMAKE_BINARY_DIR}/tests/nothofagus_test_helpers")
endif()

add_executable(visual_tests_explorer
    "visual_tests_explorer.cpp"
)
set_property(TARGET visual_tests_explorer PROPERTY CXX_STANDARD 20)
target_link_libraries(visual_tests_explorer PRIVATE nothofagus nothofagus_test_helpers)

# Default golden / actual directories point at the visual-test source tree so the
# viewer "just works" out of the box; both are overridable at runtime (argv / env
# / the in-app directory picker).
target_compile_definitions(visual_tests_explorer PRIVATE
    VISUAL_TESTS_EXPLORER_DEFAULT_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/visual/golden"
    VISUAL_TESTS_EXPLORER_DEFAULT_ACTUAL_DIR="${CMAKE_SOURCE_DIR}/tests/visual/actual"
)
