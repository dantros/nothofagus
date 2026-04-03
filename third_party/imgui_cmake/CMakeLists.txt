
set(IMGUI_PATH "../imgui/")

if(NOTHOFAGUS_HEADLESS_VULKAN)
    # Headless: no platform backend — ImGui display size is set manually
    set(IMGUI_BACKEND_SRC "")
    set(IMGUI_BACKEND_LINK "")
    set(IMGUI_BACKEND_INC "")
    set(IMGUI_BACKEND_DEFS "")
elseif(NOTHOFAGUS_WINDOW_BACKEND STREQUAL "SDL3")
    set(IMGUI_BACKEND_SRC "${IMGUI_PATH}/backends/imgui_impl_sdl3.cpp")
    set(IMGUI_BACKEND_LINK SDL3::SDL3-static)
    set(IMGUI_BACKEND_INC "")
    set(IMGUI_BACKEND_DEFS SDL_ENABLE_OLD_NAMES)
else()
    set(IMGUI_BACKEND_SRC "${IMGUI_PATH}/backends/imgui_impl_glfw.cpp")
    set(IMGUI_BACKEND_LINK glfw)
    set(IMGUI_BACKEND_INC "../glfw/include")
    set(IMGUI_BACKEND_DEFS "")
endif()

option(IMGUI_VULKAN_BACKEND "Build ImGui Vulkan backend instead of OpenGL3" OFF)

set(IMGUI_BACKEND_SOURCES "")
if(IMGUI_VULKAN_BACKEND)
    list(APPEND IMGUI_BACKEND_SOURCES "${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp")
else()
    list(APPEND IMGUI_BACKEND_SOURCES "${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp")
endif()

add_library(imgui STATIC
    "${IMGUI_PATH}/imgui.cpp"
    "${IMGUI_PATH}/imgui_demo.cpp"
    "${IMGUI_PATH}/imgui_draw.cpp"
    "${IMGUI_PATH}/imgui_tables.cpp"
    "${IMGUI_PATH}/imgui_widgets.cpp"
    "${IMGUI_BACKEND_SRC}"
    ${IMGUI_BACKEND_SOURCES}
)
set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
target_include_directories(imgui PRIVATE
    "${IMGUI_PATH}"
    ${IMGUI_BACKEND_INC}
)
if(IMGUI_BACKEND_DEFS)
    target_compile_definitions(imgui PRIVATE ${IMGUI_BACKEND_DEFS})
endif()
target_link_libraries(imgui PRIVATE ${IMGUI_BACKEND_LINK})
if(IMGUI_VULKAN_BACKEND)
    find_package(Vulkan REQUIRED)
    target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
endif()
