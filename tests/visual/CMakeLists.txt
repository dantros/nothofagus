add_executable(rendering_tests
    "rendering_tests.cpp"
)
set_property(TARGET rendering_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(rendering_tests PRIVATE nothofagus nothofagus_test_helpers Catch2::Catch2WithMain)

# Point tests at the golden directory (source tree, so updates persist) and at a
# directory to dump "actual" renders into for the Visual Tests Explorer.
#
# There is a single canonical golden set, authored with SwiftShader (deterministic
# CPU rasterizer). Every backend compares against it; the GOLDEN_DIR env var still
# overrides at runtime.
target_compile_definitions(rendering_tests PRIVATE
    NOTHOFAGUS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    NOTHOFAGUS_ACTUAL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/actual"
)

# When a SwiftShader ICD was fetched, bake its descriptor path in so the binary
# can self-select the swiftshader backend via NOTHOFAGUS_RENDER_BACKEND.
if(NOTHOFAGUS_SWIFTSHADER_ICD)
    target_compile_definitions(rendering_tests PRIVATE
        NOTHOFAGUS_SWIFTSHADER_ICD="${NOTHOFAGUS_SWIFTSHADER_ICD}")
endif()

include(Catch)
catch_discover_tests(rendering_tests)
