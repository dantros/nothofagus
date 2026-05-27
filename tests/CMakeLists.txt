add_executable(rendering_tests
    "rendering_tests.cpp"
    "golden_image.cpp"
)
set_property(TARGET rendering_tests PROPERTY CXX_STANDARD 20)
target_include_directories(rendering_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(rendering_tests PRIVATE nothofagus Catch2::Catch2WithMain)

# Pick the golden directory matching this build's render backend, so local
# ctest runs match the per-backend captures in tests/golden_*/. The
# GOLDEN_DIR env var still overrides at runtime (used by CI).
if(NOTHOFAGUS_HEADLESS_VULKAN)
    set(_golden_subdir "golden_headless_vulkan")
elseif(NOTHOFAGUS_BACKEND_VULKAN)
    # Windowed Vulkan produces the same blended pixels as headless Vulkan
    # (same blend pipeline, same swapchain format), so reuse those goldens.
    set(_golden_subdir "golden_headless_vulkan")
else()
    set(_golden_subdir "golden")  # OpenGL — historical default
endif()

target_compile_definitions(rendering_tests PRIVATE
    NOTHOFAGUS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${_golden_subdir}"
)

include(Catch)
catch_discover_tests(rendering_tests)
