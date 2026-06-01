add_executable(golden_viewer
    "golden_viewer.cpp"
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
