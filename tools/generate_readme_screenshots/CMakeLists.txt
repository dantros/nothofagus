# Re-runnable tool that renders one headless screenshot per advertised feature into
# media/feature_*.png for embedding in README.md. Reuses the shared test-helpers lib
# for DirectTexture PNG I/O (the file I/O the core library deliberately avoids). Same
# standalone pattern as tests/visual_tests_explorer — buildable without NOTHOFAGUS_BUILD_TESTS.
if(NOT TARGET nothofagus_test_helpers)
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/tests/nothofagus_test_helpers"
        "${CMAKE_BINARY_DIR}/tests/nothofagus_test_helpers")
endif()

add_executable(generate_readme_screenshots
    "generate_readme_screenshots.cpp"
)
set_property(TARGET generate_readme_screenshots PROPERTY CXX_STANDARD 20)
target_link_libraries(generate_readme_screenshots PRIVATE nothofagus nothofagus_test_helpers)

# Bake the absolute media/ output dir so the tool writes to the right place regardless
# of the working directory it is launched from.
target_compile_definitions(generate_readme_screenshots PRIVATE
    README_SCREENSHOTS_MEDIA_DIR="${CMAKE_SOURCE_DIR}/media")
