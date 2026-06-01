add_executable(rendering_tests
    "rendering_tests.cpp"
)
set_property(TARGET rendering_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(rendering_tests PRIVATE nothofagus nothofagus_test_helpers Catch2::Catch2WithMain)

# Point tests at the golden directory (source tree, so updates persist) and at a
# directory to dump "actual" renders into for the golden viewer.
target_compile_definitions(rendering_tests PRIVATE
    NOTHOFAGUS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    NOTHOFAGUS_ACTUAL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/actual"
)

include(Catch)
catch_discover_tests(rendering_tests)
