add_executable(rendering_tests
    "rendering_tests.cpp"
    "golden_image.cpp"
)
set_property(TARGET rendering_tests PROPERTY CXX_STANDARD 20)
target_include_directories(rendering_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(rendering_tests PRIVATE nothofagus Catch2::Catch2WithMain)

# Point tests at the golden directory (source tree, so updates persist).
target_compile_definitions(rendering_tests PRIVATE
    NOTHOFAGUS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
)

include(Catch)
catch_discover_tests(rendering_tests)
