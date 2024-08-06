
set(IMGUI_PATH "../imgui/")
set(GLFW_PATH "../glfw/include")

add_library(imgui STATIC
    "${IMGUI_PATH}/imgui.cpp"
    "${IMGUI_PATH}/imgui_demo.cpp"
    "${IMGUI_PATH}/imgui_draw.cpp"
    "${IMGUI_PATH}/imgui_tables.cpp"
    "${IMGUI_PATH}/imgui_widgets.cpp"
    "${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_PATH}/backends/imgui_impl_glfw.cpp"
)
set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
target_include_directories(imgui PRIVATE
    "${IMGUI_PATH}"
    "${GLFW_PATH}"
)