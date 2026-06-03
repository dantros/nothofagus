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

# When the visual tests are part of this build (e.g. the *-sdl3-vulkan-tests
# presets), point the "Generate actual images" button at the rendering_tests
# binary and ensure it is built alongside the explorer. Without the visual
# tests, the button falls back to the RENDERING_TESTS_BIN env var. This is the
# windowed (GPU) lane used for the "auto" backend.
if(TARGET rendering_tests)
    add_dependencies(visual_tests_explorer rendering_tests)
    target_compile_definitions(visual_tests_explorer PRIVATE
        VISUAL_TESTS_EXPLORER_TESTS_BIN="$<TARGET_FILE:rendering_tests>")
endif()

# Headless (pure-offscreen) rendering_tests for the explorer's backend toggle.
#
# The explorer is a windowed app and cannot be compiled NOTHOFAGUS_HEADLESS_VULKAN
# (no window backend), but SwiftShader only renders robustly in the headless,
# swapchain-free path. A single *headless* rendering_tests, however, drives BOTH
# the real GPU and SwiftShader by toggling the Vulkan ICD — so we build one here
# as a separate ExternalProject (pattern mirrors the VMA ExternalProject in the
# top-level CMakeLists) and hand its path to the explorer. The explorer then
# shells out to it with NOTHOFAGUS_RENDER_BACKEND=gpu|swiftshader.
include(ExternalProject)
set(_headless_tests_bin
    "${CMAKE_BINARY_DIR}/headless_tests_build/tests/visual/rendering_tests${CMAKE_EXECUTABLE_SUFFIX}")
ExternalProject_Add(headless_rendering_tests
    SOURCE_DIR  "${CMAKE_SOURCE_DIR}"
    BINARY_DIR  "${CMAKE_BINARY_DIR}/headless_tests_build"
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DNOTHOFAGUS_BACKEND_VULKAN=ON
        -DNOTHOFAGUS_HEADLESS_VULKAN=ON
        -DNOTHOFAGUS_FETCH_SWIFTSHADER=ON
        -DNOTHOFAGUS_BUILD_TESTS=ON
        -DNOTHOFAGUS_BUILD_TESTS_VISUAL=ON
        -DNOTHOFAGUS_BUILD_TESTS_NONVISUAL=OFF
        -DNOTHOFAGUS_BUILD_EXAMPLES=OFF
        -DNOTHOFAGUS_INSTALL=OFF
        -DNOTHOFAGUS_USE_GLSLANG=${NOTHOFAGUS_USE_GLSLANG}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target rendering_tests
    INSTALL_COMMAND ""
    BUILD_ALWAYS ON
    BUILD_BYPRODUCTS "${_headless_tests_bin}")
add_dependencies(visual_tests_explorer headless_rendering_tests)
target_compile_definitions(visual_tests_explorer PRIVATE
    VISUAL_TESTS_EXPLORER_HEADLESS_TESTS_BIN="${_headless_tests_bin}")
