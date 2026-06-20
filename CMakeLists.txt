cmake_minimum_required(VERSION 3.26)

project(nothofagus_project CXX C)

# ─────────────────────────────────────────────
# Graphics backend selection
# ─────────────────────────────────────────────
option(NOTHOFAGUS_BACKEND_VULKAN "Use Vulkan backend instead of OpenGL" OFF)
option(NOTHOFAGUS_HEADLESS_VULKAN "Pure offscreen Vulkan rendering (no window/display server)" OFF)
option(NOTHOFAGUS_VMA_USE_CL "Windows only: compile VMA implementation unit with MSVC cl.exe (avoids clang-cl warnings)" OFF)
option(NOTHOFAGUS_FETCH_SWIFTSHADER "Download a prebuilt SwiftShader Vulkan ICD at configure time for deterministic CPU rendering" OFF)

if(NOTHOFAGUS_HEADLESS_VULKAN AND NOT NOTHOFAGUS_BACKEND_VULKAN)
    message(FATAL_ERROR "NOTHOFAGUS_HEADLESS_VULKAN requires NOTHOFAGUS_BACKEND_VULKAN=ON")
endif()

# ─────────────────────────────────────────────
# Window backend selection
# ─────────────────────────────────────────────
set(NOTHOFAGUS_WINDOW_BACKEND "GLFW" CACHE STRING "Window/input backend: GLFW or SDL3")
set_property(CACHE NOTHOFAGUS_WINDOW_BACKEND PROPERTY STRINGS "GLFW" "SDL3")

if(NOTHOFAGUS_HEADLESS_VULKAN)
    # ── Headless (no window library) ─────────
    set(NOTHOFAGUS_WIN_LINK "")
    set(NOTHOFAGUS_WIN_INCS "")
    set(NOTHOFAGUS_WIN_DEFS NOTHOFAGUS_HEADLESS_VULKAN)
    set(NOTHOFAGUS_BACKEND_SRC
        "source/backends/headless_backend.cpp")
elseif(NOTHOFAGUS_WINDOW_BACKEND STREQUAL "SDL3")
    # ── SDL3 ──────────────────────────────────
    set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
    option(NOTHOFAGUS_ENABLE_X11 "Enable X11 support in SDL3" OFF)
    if(NOT NOTHOFAGUS_ENABLE_X11)
        set(SDL_X11 OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("third_party/SDL")

    set(NOTHOFAGUS_WIN_LINK SDL3::SDL3-static)
    set(NOTHOFAGUS_WIN_INCS "third_party/SDL/include")
    set(NOTHOFAGUS_WIN_DEFS NOTHOFAGUS_BACKEND_SDL3)
    set(NOTHOFAGUS_BACKEND_SRC
        "source/backends/sdl3_backend.cpp"
        "source/backends/sdl3_keyboard.cpp"
        "source/backends/sdl3_mouse.cpp"
        "source/backends/sdl3_gamepad.cpp")
else()
    # ── GLFW (default) ────────────────────────
    option(GLFW_INSTALL        "Disabling installation of GLFW"   OFF)
    option(BUILD_SHARED_LIBS   "Build shared libraries"           OFF)
    option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs"  OFF)
    option(GLFW_BUILD_TESTS    "Build the GLFW test programs"     OFF)
    option(GLFW_BUILD_DOCS     "Build the GLFW documentation"     OFF)
    option(NOTHOFAGUS_ENABLE_X11 "Enable X11 support in GLFW" OFF)
    if(NOT NOTHOFAGUS_ENABLE_X11)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("third_party/glfw")

    set(NOTHOFAGUS_WIN_LINK glfw)
    set(NOTHOFAGUS_WIN_INCS "third_party/glfw/include")
    set(NOTHOFAGUS_WIN_DEFS "")
    set(NOTHOFAGUS_BACKEND_SRC
        "source/backends/glfw_backend.cpp"
        "source/backends/glfw_keyboard.cpp"
        "source/backends/glfw_mouse.cpp"
        "source/backends/glfw_gamepad.cpp")
endif()

add_subdirectory("third_party/glad")
if(NOTHOFAGUS_BACKEND_VULKAN)
    set(IMGUI_VULKAN_BACKEND ON CACHE BOOL "" FORCE)
endif()
add_subdirectory("third_party/imgui_cmake")

file(GLOB_RECURSE NOTHOFAGUS_SOURCE_HEADERS "source/*.h")
file(GLOB_RECURSE NOTHOFAGUS_PUBLIC_HEADERS "include/*.h")

add_library(nothofagus
    "source/canvas.cpp"
    "source/frame_runner.cpp"
    "source/asset_registry.cpp"
    "source/texture_container.cpp"
    "source/mesh_container.cpp"
    "source/render_target_container.cpp"
    "source/imgui_rtt_manager.cpp"
    "source/imgui_image_manager.cpp"
    "source/imgui_font_manager.cpp"
    ${NOTHOFAGUS_BACKEND_SRC}
    "source/bellota_to_mesh.cpp"
    "source/mesh.cpp"
    "source/transform.cpp"
    "source/texture.cpp"
    "source/resource_usage_monitor.cpp"
    "source/performance_monitor.cpp"
    "source/controller.cpp"
    "source/keyboard.cpp"
    "source/mouse.cpp"
    "source/gamepad.cpp"
    "source/text.cpp"
    "source/animation_state_machine.cpp"
    "source/animation_state.cpp"
    "source/dense_land.cpp"
    "source/sparse_land.cpp"
    "source/explorer_manager.cpp"
    "source/markdown_renderer.cpp"

    # This is only required to show header files in the Visual Studio IDE solution browser
    ${NOTHOFAGUS_SOURCE_HEADERS}
    ${NOTHOFAGUS_PUBLIC_HEADERS}
)

# ── Embedded font family — Noto Sans (always) + optional CJK (compressed) ────
# Every face in assets/ is stb-compressed at build time by the vendored
# binary_to_compressed_c tool and decompressed at runtime via ImGui's
# AddFontFromMemoryCompressedTTF (no runtime deps). Generated output lands
# under build/<preset>/generated/ (already gitignored). Compression is
# incremental: a face re-runs only when its .ttf, the compressor tool, or the
# wrapper script changes (DEPENDS edges) — never on a bare reconfigure.
set(NOTHOFAGUS_FONT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${NOTHOFAGUS_FONT_GEN_DIR}")

# Opt-in CJK scripts (default OFF). The aggregate fans out to the per-script
# flags. When all are OFF, no CJK .cpp is generated/compiled and no
# NOTHOFAGUS_HAS_CJK_* define is set — zero binary/compile cost.
option(NOTHOFAGUS_EMBED_CJK    "Embed all CJK scripts (SC+TC+JP+KR)" OFF)
option(NOTHOFAGUS_EMBED_CJK_SC "Embed Simplified Chinese font"  OFF)
option(NOTHOFAGUS_EMBED_CJK_TC "Embed Traditional Chinese font" OFF)
option(NOTHOFAGUS_EMBED_CJK_JP "Embed Japanese font"            OFF)
option(NOTHOFAGUS_EMBED_CJK_KR "Embed Korean font"              OFF)
if(NOTHOFAGUS_EMBED_CJK)
    set(NOTHOFAGUS_EMBED_CJK_SC ON)
    set(NOTHOFAGUS_EMBED_CJK_TC ON)
    set(NOTHOFAGUS_EMBED_CJK_JP ON)
    set(NOTHOFAGUS_EMBED_CJK_KR ON)
endif()

# Host tool that compresses a binary into a C array (vendored from imgui).
# TODO(cross-compile): built for the host, which equals the target for
# nothofagus's native builds. A future cross build would need this tool built
# with a host toolchain (ExternalProject) instead.
add_executable(nothofagus_font_compressor
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/fonts/binary_to_compressed_c.cpp")
set_target_properties(nothofagus_font_compressor PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# Face entries: "<ttf>|<symbol>|<out.cpp>[|<HAS_DEFINE>]". The <symbol> string
# is the single source of truth driving BOTH the generated
# <symbol>_compressed_data[]/<symbol>_compressed_size names and the extern
# decls in embedded_fonts.h. Latin faces are always embedded; CJK faces are
# appended per enabled option (and carry a NOTHOFAGUS_HAS_CJK_* define).
set(NOTHOFAGUS_FONT_FACES
    "NotoSans-Regular.ttf|notoSansRegular|noto_sans_regular.cpp"
    "NotoSans-Bold.ttf|notoSansBold|noto_sans_bold.cpp"
    "NotoSans-Italic.ttf|notoSansItalic|noto_sans_italic.cpp"
    "NotoSans-BoldItalic.ttf|notoSansBoldItalic|noto_sans_bolditalic.cpp"
    "NotoSansMono-Regular.ttf|notoSansMono|noto_sans_mono.cpp"
)
if(NOTHOFAGUS_EMBED_CJK_SC)
    list(APPEND NOTHOFAGUS_FONT_FACES "NotoSansSC-Regular.ttf|notoSansCjkSc|noto_cjk_sc.cpp|NOTHOFAGUS_HAS_CJK_SC")
endif()
if(NOTHOFAGUS_EMBED_CJK_TC)
    list(APPEND NOTHOFAGUS_FONT_FACES "NotoSansTC-Regular.ttf|notoSansCjkTc|noto_cjk_tc.cpp|NOTHOFAGUS_HAS_CJK_TC")
endif()
if(NOTHOFAGUS_EMBED_CJK_JP)
    list(APPEND NOTHOFAGUS_FONT_FACES "NotoSansJP-Regular.ttf|notoSansCjkJp|noto_cjk_jp.cpp|NOTHOFAGUS_HAS_CJK_JP")
endif()
if(NOTHOFAGUS_EMBED_CJK_KR)
    list(APPEND NOTHOFAGUS_FONT_FACES "NotoSansKR-Regular.ttf|notoSansCjkKr|noto_cjk_kr.cpp|NOTHOFAGUS_HAS_CJK_KR")
endif()

set(NOTHOFAGUS_FONT_GEN_SRCS "")
set(_font_extern_decls "")
foreach(_face IN LISTS NOTHOFAGUS_FONT_FACES)
    string(REPLACE "|" ";" _parts "${_face}")
    list(GET _parts 0 _ttf)
    list(GET _parts 1 _sym)
    list(GET _parts 2 _cpp)
    list(LENGTH _parts _plen)
    set(_in  "${CMAKE_CURRENT_SOURCE_DIR}/assets/${_ttf}")
    set(_out "${NOTHOFAGUS_FONT_GEN_DIR}/${_cpp}")
    add_custom_command(
        OUTPUT "${_out}"
        COMMAND ${CMAKE_COMMAND}
            "-DTOOL=$<TARGET_FILE:nothofagus_font_compressor>"
            "-DIN_TTF=${_in}" "-DOUT_CPP=${_out}"
            "-DSYM=${_sym}" "-DHEADER_NAME=embedded_fonts.h"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_font_compressor.cmake"
        DEPENDS "${_in}" nothofagus_font_compressor
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_font_compressor.cmake"
        COMMENT "Compressing font ${_ttf} -> generated/${_cpp}"
        VERBATIM)
    list(APPEND NOTHOFAGUS_FONT_GEN_SRCS "${_out}")
    string(APPEND _font_extern_decls
        "extern const unsigned char ${_sym}_compressed_data[];\n"
        "extern const unsigned int  ${_sym}_compressed_size;\n")
    if(_plen GREATER 3)
        list(GET _parts 3 _def)
        target_compile_definitions(nothofagus PRIVATE ${_def})
    endif()
endforeach()

# Declarations header. file(GENERATE) only rewrites on content change, so a
# bare reconfigure does not bump its mtime and force a canvas.cpp recompile.
file(GENERATE OUTPUT "${NOTHOFAGUS_FONT_GEN_DIR}/embedded_fonts.h"
    CONTENT "#pragma once
// AUTO-GENERATED by CMake. Compressed embedded-font blob declarations;
// definitions live in the generated noto_*.cpp translation units.
${_font_extern_decls}")

set_source_files_properties(${NOTHOFAGUS_FONT_GEN_SRCS} PROPERTIES GENERATED TRUE)
target_sources(nothofagus PRIVATE ${NOTHOFAGUS_FONT_GEN_SRCS})
target_include_directories(nothofagus PRIVATE "${NOTHOFAGUS_FONT_GEN_DIR}")
add_custom_target(nothofagus_fonts DEPENDS ${NOTHOFAGUS_FONT_GEN_SRCS})
add_dependencies(nothofagus nothofagus_fonts)

if(NOTHOFAGUS_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)
    add_subdirectory("third_party/vk-bootstrap")

    # Optionally fetch a prebuilt SwiftShader Vulkan ICD for deterministic CPU
    # rendering (used by the visual tests to match CI). Invoked at directory
    # scope so the resulting paths reach the sibling tests/ subdirectories.
    if(NOTHOFAGUS_FETCH_SWIFTSHADER)
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_swiftshader.cmake")
        nothofagus_fetch_swiftshader(NOTHOFAGUS_SWIFTSHADER_LIB NOTHOFAGUS_SWIFTSHADER_ICD)
    endif()

    # VMA: header-only interface target
    add_library(VulkanMemoryAllocator INTERFACE)
    target_include_directories(VulkanMemoryAllocator INTERFACE
        "third_party/VulkanMemoryAllocator/include")

    target_compile_definitions(nothofagus PRIVATE NOTHOFAGUS_BACKEND_VULKAN)
    target_sources(nothofagus PRIVATE
        "source/backends/vulkan_backend.cpp"
        "source/backends/vulkan_presentation.cpp"
    )

    # Build VMA as a separate ExternalProject using the same compiler, with all warnings
    # suppressed (-w). Works on both Windows (clang-cl) and Linux (clang++).
    include(ExternalProject)
    set(_vma_install_dir "${CMAKE_CURRENT_BINARY_DIR}/vma_install")
    if(WIN32)
        set(_vma_lib "${_vma_install_dir}/lib/vma_impl.lib")
    else()
        set(_vma_lib "${_vma_install_dir}/lib/libvma_impl.a")
    endif()

    ExternalProject_Add(vma_project
        SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator_project"
        BINARY_DIR  "${CMAKE_CURRENT_BINARY_DIR}/vma_build"
        INSTALL_DIR "${_vma_install_dir}"
        CMAKE_ARGS
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
            "-DCMAKE_CXX_FLAGS=-w"
            "-DVMA_IMPL_SRC=${CMAKE_CURRENT_SOURCE_DIR}/source/backends/vma_impl.cpp"
            "-DVMA_INCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include"
            "-DVMA_VULKAN_INCLUDE_DIR=${Vulkan_INCLUDE_DIRS}"
        BUILD_BYPRODUCTS "${_vma_lib}"
    )

    add_library(vma_impl_project STATIC IMPORTED GLOBAL)
    set_target_properties(vma_impl_project PROPERTIES IMPORTED_LOCATION "${_vma_lib}")
    add_dependencies(vma_impl_project vma_project)
    add_dependencies(nothofagus vma_project)
    target_link_libraries(nothofagus PRIVATE vma_impl_project)

    # SPIR-V shader compilation
    option(NOTHOFAGUS_USE_GLSLANG "Use glslangValidator instead of glslc for shader compilation" OFF)

    set(shaderVertSrc "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite.vert")
    set(shaderFragSrc "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite.frag")
    set(shaderIndirectFragSrc "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite_indirect.frag")
    set(shaderTilemapFragSrc  "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite_tilemap.frag")
    set(SHADER_VERT_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv")
    set(SHADER_FRAG_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv")
    set(SHADER_INDIRECT_FRAG_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite_indirect.frag.spv")
    set(SHADER_TILEMAP_FRAG_SPV  "${CMAKE_CURRENT_BINARY_DIR}/sprite_tilemap.frag.spv")

    if(NOTHOFAGUS_USE_GLSLANG)
        find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
        add_custom_command(OUTPUT "${SHADER_VERT_SPV}"
            COMMAND "${GLSLANG_VALIDATOR}" -V "${shaderVertSrc}" -o "${SHADER_VERT_SPV}"
            DEPENDS "${shaderVertSrc}" VERBATIM)
        add_custom_command(OUTPUT "${SHADER_FRAG_SPV}"
            COMMAND "${GLSLANG_VALIDATOR}" -V "${shaderFragSrc}" -o "${SHADER_FRAG_SPV}"
            DEPENDS "${shaderFragSrc}" VERBATIM)
        add_custom_command(OUTPUT "${SHADER_INDIRECT_FRAG_SPV}"
            COMMAND "${GLSLANG_VALIDATOR}" -V "${shaderIndirectFragSrc}" -o "${SHADER_INDIRECT_FRAG_SPV}"
            DEPENDS "${shaderIndirectFragSrc}" VERBATIM)
        add_custom_command(OUTPUT "${SHADER_TILEMAP_FRAG_SPV}"
            COMMAND "${GLSLANG_VALIDATOR}" -V "${shaderTilemapFragSrc}" -o "${SHADER_TILEMAP_FRAG_SPV}"
            DEPENDS "${shaderTilemapFragSrc}" VERBATIM)
    else()
        find_program(GLSLC glslc REQUIRED)
        add_custom_command(OUTPUT "${SHADER_VERT_SPV}"
            COMMAND "${GLSLC}" "${shaderVertSrc}" -o "${SHADER_VERT_SPV}"
            DEPENDS "${shaderVertSrc}" VERBATIM)
        add_custom_command(OUTPUT "${SHADER_FRAG_SPV}"
            COMMAND "${GLSLC}" "${shaderFragSrc}" -o "${SHADER_FRAG_SPV}"
            DEPENDS "${shaderFragSrc}" VERBATIM)
        add_custom_command(OUTPUT "${SHADER_INDIRECT_FRAG_SPV}"
            COMMAND "${GLSLC}" "${shaderIndirectFragSrc}" -o "${SHADER_INDIRECT_FRAG_SPV}"
            DEPENDS "${shaderIndirectFragSrc}" VERBATIM)
        add_custom_command(OUTPUT "${SHADER_TILEMAP_FRAG_SPV}"
            COMMAND "${GLSLC}" "${shaderTilemapFragSrc}" -o "${SHADER_TILEMAP_FRAG_SPV}"
            DEPENDS "${shaderTilemapFragSrc}" VERBATIM)
    endif()
    add_custom_target(spirv_shaders DEPENDS "${SHADER_VERT_SPV}" "${SHADER_FRAG_SPV}" "${SHADER_INDIRECT_FRAG_SPV}" "${SHADER_TILEMAP_FRAG_SPV}")
    add_dependencies(nothofagus spirv_shaders)
    target_compile_definitions(nothofagus PRIVATE
        NOTHOFAGUS_SPRITE_VERT_SPV="${SHADER_VERT_SPV}"
        NOTHOFAGUS_SPRITE_FRAG_SPV="${SHADER_FRAG_SPV}"
        NOTHOFAGUS_SPRITE_INDIRECT_FRAG_SPV="${SHADER_INDIRECT_FRAG_SPV}"
        NOTHOFAGUS_SPRITE_TILEMAP_FRAG_SPV="${SHADER_TILEMAP_FRAG_SPV}")

    target_link_libraries(nothofagus PRIVATE
        Vulkan::Vulkan vk-bootstrap::vk-bootstrap VulkanMemoryAllocator)
    target_include_directories(nothofagus PRIVATE
        "third_party/VulkanMemoryAllocator/include"
        "third_party/vk-bootstrap/src")
else()
    target_sources(nothofagus PRIVATE
        "source/backends/opengl_backend.cpp"
        "source/backends/opengl_mesh.cpp"
        "source/backends/opengl_render_target.cpp"
        "source/backends/opengl_texture.cpp"
    )
endif()

option(NOTHOFAGUS_ENABLE_TRACY "Enable Tracy profiler zones (consumer must link TracyClient)" OFF)
if(NOTHOFAGUS_ENABLE_TRACY)
    target_compile_definitions(nothofagus PUBLIC TRACY_ENABLE)
endif()

set_property(TARGET nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(nothofagus
    PUBLIC
        "include"
        "third_party/glm"
        "third_party/imgui"
        "third_party/imgui-filebrowser"
        "third_party/spdlog/include"
    PRIVATE
        "source"
        "third_party/glad/include"
        ${NOTHOFAGUS_WIN_INCS}
        "third_party/font8x8"
        "third_party/imgui_md"
        "third_party/md4c/src"
)
target_link_libraries(nothofagus PRIVATE glad ${NOTHOFAGUS_WIN_LINK} imgui)
# spdlog uses fmt to format string, this library requires /utf-8 when using MSVC
# The issue has not been fixed. More info at: https://github.com/gabime/spdlog/issues/3251
# diabling FMT_UNICODE is a workaround.
if(MSVC)
    target_compile_definitions(nothofagus PRIVATE FMT_UNICODE=0)
endif()
if(NOTHOFAGUS_WIN_DEFS)
    target_compile_definitions(nothofagus PRIVATE ${NOTHOFAGUS_WIN_DEFS})
endif()

option(NOTHOFAGUS_BUILD_EXAMPLES "Build Nothofagus examples?" OFF)
if (${NOTHOFAGUS_BUILD_EXAMPLES})
    add_subdirectory("examples")
endif()

option(NOTHOFAGUS_BUILD_TESTS "Build Nothofagus tests?" OFF)
if (${NOTHOFAGUS_BUILD_TESTS})
    include(CTest)
    add_subdirectory("tests")
endif()

# Visual Tests Explorer: a windowed Nothofagus tool to inspect golden/actual/diff
# images and update goldens. Pulls in stb_image_plus for file I/O. OFF by default
# (interactive tool, not part of the headless CI lane).
option(NOTHOFAGUS_BUILD_VISUAL_TESTS_EXPLORER "Build the Visual Tests Explorer tool?" OFF)
if (${NOTHOFAGUS_BUILD_VISUAL_TESTS_EXPLORER})
    add_subdirectory("tests/visual_tests_explorer")
endif()

option(NOTHOFAGUS_BUILD_DOCS "Build Nothofagus documentation? [WIP]" OFF)
if (${NOTHOFAGUS_BUILD_DOCS})
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        add_custom_target(
            doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
            COMMENT "Generating Doxygen documentation"
            VERBATIM
        )
    else()
        message(STATUS "Doxygen not found. Unable to generate doxygen documentation.")
    endif()
endif()

option(NOTHOFAGUS_INSTALL "Do you whish to install Nothofagus static library?" OFF)
if (${NOTHOFAGUS_INSTALL})
    include(GNUInstallDirs)

    # Installing binaries: executables and static libraries
    install(TARGETS nothofagus
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT Nothofagus_Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Nothofagus_Runtime
            NAMELINK_COMPONENT Nothofagus_Development
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Nothofagus_Development
    )

    # Installing public headers
    target_sources(nothofagus PUBLIC
        FILE_SET nothofagus_Headers
        TYPE HEADERS
        BASE_DIRS include
        FILES ${NOTHOFAGUS_PUBLIC_HEADERS}
    )
    install(TARGETS nothofagus FILE_SET nothofagus_Headers)

    # Installing examples
    if (${NOTHOFAGUS_BUILD_EXAMPLES})
        install(TARGETS
            hello_nothofagus
            hello_text
            hello_tint
            test_keyboard
            hello_layers
            hello_animation
            hello_animation_state_machine
            hello_render_to_texture
            hello_nested_render_targets
            hello_screenshot
            hello_headless
            hello_tilemap
            hello_dense_land
            hello_sparse_land
            test_gamepad
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT NothofagusExamples_Runtime
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT NothofagusExamples_Runtime
                NAMELINK_COMPONENT NothofagusExamples_Development
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT NothofagusExamples_Development
        )
    endif()

    # Installing documentation
    if (${NOTHOFAGUS_BUILD_DOCS})
        # TODO
    endif()
endif()
