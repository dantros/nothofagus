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

# Present-mode regression tests. Two executables (one Vulkan instance per Canvas,
# and vk-bootstrap caches the debug-messenger fn pointer per process, so the
# messenger-using sync test runs in its own process). Meaningful on windowed Vulkan;
# the pixel test trivially passes and the sync test SKIPs under the headless backend.
# No golden images or test-helpers needed (raw-byte / message-set compare).
add_executable(present_mode_pixel_tests
    "present_mode_pixel_tests.cpp"
)
set_property(TARGET present_mode_pixel_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(present_mode_pixel_tests PRIVATE nothofagus Catch2::Catch2WithMain)

add_executable(present_mode_sync_tests
    "present_mode_sync_tests.cpp"
)
set_property(TARGET present_mode_sync_tests PROPERTY CXX_STANDARD 20)
target_link_libraries(present_mode_sync_tests PRIVATE nothofagus Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(rendering_tests)
catch_discover_tests(present_mode_pixel_tests)
catch_discover_tests(present_mode_sync_tests)
