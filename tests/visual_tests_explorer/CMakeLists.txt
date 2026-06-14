# Interactive (windowed) tool to explore visual-test results: for each test case it
# shows the single canonical golden plus every render backend's actual and the
# per-backend diff. The explorer GUI links the in-tree (windowed) nothofagus, so it
# requires a windowed configuration.
if(NOTHOFAGUS_HEADLESS_VULKAN)
    message(FATAL_ERROR
        "NOTHOFAGUS_BUILD_VISUAL_TESTS_EXPLORER requires a windowed configuration; "
        "it cannot be built with NOTHOFAGUS_HEADLESS_VULKAN=ON (no window backend).")
endif()

# The test-helpers lib provides DirectTexture PNG I/O + comparison; pull it in if
# the test tree didn't already (this can be built without NOTHOFAGUS_BUILD_TESTS).
if(NOT TARGET nothofagus_test_helpers)
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/nothofagus_test_helpers" "${CMAKE_BINARY_DIR}/tests/nothofagus_test_helpers")
endif()

add_executable(visual_tests_explorer
    "visual_tests_explorer.cpp"
)
set_property(TARGET visual_tests_explorer PROPERTY CXX_STANDARD 20)
target_link_libraries(visual_tests_explorer PRIVATE nothofagus nothofagus_test_helpers)

# ── Test-render binaries for every backend, built independently of the main
#    nothofagus configuration ───────────────────────────────────────────────
# The explorer shells out to these to produce each backend's "actual" PNGs. Each
# is a separate ExternalProject that configures the repo with a fixed backend
# (pattern mirrors the VMA ExternalProject in the top-level CMakeLists), so the
# set of backends the explorer can show does NOT depend on the main CMake options
# — only on what is actually available on this machine. The explorer GUI's own
# backend (whatever windowed config built it) is irrelevant to these.
include(ExternalProject)

# OpenGL lane — always built. Renders offscreen via a hidden window, so it needs a
# display server at runtime (the windowed explorer always has one).
set(_vte_opengl_tests_bin
    "${CMAKE_BINARY_DIR}/vte_opengl_build/tests/visual/rendering_tests${CMAKE_EXECUTABLE_SUFFIX}")
ExternalProject_Add(vte_opengl_rendering_tests
    SOURCE_DIR  "${CMAKE_SOURCE_DIR}"
    BINARY_DIR  "${CMAKE_BINARY_DIR}/vte_opengl_build"
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DNOTHOFAGUS_BACKEND_VULKAN=OFF
        -DNOTHOFAGUS_WINDOW_BACKEND=${NOTHOFAGUS_WINDOW_BACKEND}
        -DNOTHOFAGUS_ENABLE_X11=${NOTHOFAGUS_ENABLE_X11}
        -DNOTHOFAGUS_BUILD_TESTS=ON
        -DNOTHOFAGUS_BUILD_TESTS_VISUAL=ON
        -DNOTHOFAGUS_BUILD_TESTS_NONVISUAL=OFF
        -DNOTHOFAGUS_BUILD_EXAMPLES=OFF
        -DNOTHOFAGUS_INSTALL=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target rendering_tests
    INSTALL_COMMAND ""
    BUILD_ALWAYS ON
    BUILD_BYPRODUCTS "${_vte_opengl_tests_bin}")
add_dependencies(visual_tests_explorer vte_opengl_rendering_tests)

# Vulkan lane — only when a Vulkan SDK is present on this machine. Headless
# (pure-offscreen) so a single binary drives both the system GPU and the fetched
# SwiftShader ICD by toggling NOTHOFAGUS_RENDER_BACKEND.
find_package(Vulkan QUIET)
set(_vte_vulkan_tests_bin "")
set(_vte_swiftshader_available 0)
if(Vulkan_FOUND)
    set(_vte_vulkan_tests_bin
        "${CMAKE_BINARY_DIR}/vte_vulkan_build/tests/visual/rendering_tests${CMAKE_EXECUTABLE_SUFFIX}")
    ExternalProject_Add(vte_vulkan_rendering_tests
        SOURCE_DIR  "${CMAKE_SOURCE_DIR}"
        BINARY_DIR  "${CMAKE_BINARY_DIR}/vte_vulkan_build"
        CMAKE_ARGS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DNOTHOFAGUS_BACKEND_VULKAN=ON
            -DNOTHOFAGUS_HEADLESS_VULKAN=ON
            -DNOTHOFAGUS_FETCH_SWIFTSHADER=ON
            -DNOTHOFAGUS_USE_GLSLANG=${NOTHOFAGUS_USE_GLSLANG}
            -DNOTHOFAGUS_BUILD_TESTS=ON
            -DNOTHOFAGUS_BUILD_TESTS_VISUAL=ON
            -DNOTHOFAGUS_BUILD_TESTS_NONVISUAL=OFF
            -DNOTHOFAGUS_BUILD_EXAMPLES=OFF
            -DNOTHOFAGUS_INSTALL=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target rendering_tests
        INSTALL_COMMAND ""
        BUILD_ALWAYS ON
        BUILD_BYPRODUCTS "${_vte_vulkan_tests_bin}")
    add_dependencies(visual_tests_explorer vte_vulkan_rendering_tests)

    # SwiftShader prebuilt exists only for linux-x86_64 (mirrors cmake/fetch_swiftshader.cmake).
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(_vte_swiftshader_available 1)
    endif()
endif()

# Bake every path + availability flag into the explorer; it reads no env vars. The
# per-backend actual dirs live under the source tree (gitignored via actual_*/).
target_compile_definitions(visual_tests_explorer PRIVATE
    VTE_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/visual/golden"
    VTE_OPENGL_TESTS_BIN="${_vte_opengl_tests_bin}"
    VTE_VULKAN_TESTS_BIN="${_vte_vulkan_tests_bin}"
    VTE_SWIFTSHADER_AVAILABLE=${_vte_swiftshader_available}
    VTE_ACTUAL_DIR_OPENGL_GPU="${CMAKE_SOURCE_DIR}/tests/visual/actual_opengl_gpu"
    VTE_ACTUAL_DIR_VULKAN_GPU="${CMAKE_SOURCE_DIR}/tests/visual/actual_vulkan_gpu"
    VTE_ACTUAL_DIR_VULKAN_SWIFTSHADER="${CMAKE_SOURCE_DIR}/tests/visual/actual_vulkan_swiftshader")
