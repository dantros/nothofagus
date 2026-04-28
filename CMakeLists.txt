cmake_minimum_required(VERSION 3.26)

project(nothofagus_project CXX)

# ─────────────────────────────────────────────
# Graphics backend selection
# ─────────────────────────────────────────────
option(NOTHOFAGUS_BACKEND_VULKAN "Use Vulkan backend instead of OpenGL" OFF)
option(NOTHOFAGUS_HEADLESS_VULKAN "Pure offscreen Vulkan rendering (no window/display server)" OFF)
option(NOTHOFAGUS_VMA_USE_CL "Windows only: compile VMA implementation unit with MSVC cl.exe (avoids clang-cl warnings)" OFF)

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
    "source/canvas_impl.cpp"
    "source/imgui_rtt_manager.cpp"
    ${NOTHOFAGUS_BACKEND_SRC}
    "source/bellota_to_mesh.cpp"
    "source/mesh.cpp"
    "source/transform.cpp"
    "source/texture.cpp"
    "source/texture_usage_monitor.cpp"
    "source/performance_monitor.cpp"
    "source/controller.cpp"
    "source/keyboard.cpp"
    "source/mouse.cpp"
    "source/gamepad.cpp"
    "source/text.cpp"
    "source/animation_state_machine.cpp"
    "source/animation_state.cpp"

    # This is only required to show header files in the Visual Studio IDE solution browser
    ${NOTHOFAGUS_SOURCE_HEADERS}
    ${NOTHOFAGUS_PUBLIC_HEADERS}
)

if(NOTHOFAGUS_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)
    add_subdirectory("third_party/vk-bootstrap")

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
        "third_party/spdlog/include"
    PRIVATE
        "source"
        "third_party/glad/include"
        ${NOTHOFAGUS_WIN_INCS}
        "third_party/font8x8"
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
    add_subdirectory("third_party/Catch2")
    include(CTest)
    add_subdirectory("tests")
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
