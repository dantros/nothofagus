
set(IMGUI_PATH "../imgui/")

add_library(imgui STATIC
    "${IMGUI_PATH}/imgui.cpp"
    "${IMGUI_PATH}/imgui_demo.cpp"
    "${IMGUI_PATH}/imgui_draw.cpp"
    "${IMGUI_PATH}/imgui_tables.cpp"
    "${IMGUI_PATH}/imgui_widgets.cpp"
    "${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_PATH}/backends/imgui_impl_sdl3.cpp"
)
set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
target_include_directories(imgui PRIVATE "${IMGUI_PATH}")
target_compile_definitions(imgui PRIVATE SDL_ENABLE_OLD_NAMES)
target_link_libraries(imgui PRIVATE SDL3::SDL3-static)
