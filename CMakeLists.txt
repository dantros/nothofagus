cmake_minimum_required(VERSION 3.20)
set(CMAKE_LEGACY_CYGWIN_WIN32 OFF)

project(cpp_opengl_app_project CXX)

# Setting up build options for GLFW
option(GLFW_INSTALL "Disabling installation of GLFW" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
add_subdirectory("third_party/glfw")

add_subdirectory("third_party/whereami2cpp")
add_subdirectory("third_party/glad")
add_subdirectory("third_party/imgui_cmake")

add_executable(cpp_opengl_app "source/main.cpp")
set_property(TARGET cpp_opengl_app PROPERTY CXX_STANDARD 20)
target_include_directories(cpp_opengl_app PRIVATE
    "third_party/argparse/include"
    "third_party/whereami2cpp/source"
    "third_party/spdlog/include"
    "third_party/json/include"
    "third_party/glad/include"
    "third_party/glfw/include"
    "third_party/glm"
    "third_party/imgui"
)
target_link_libraries(cpp_opengl_app PRIVATE whereami2cpp glad glfw imgui)

include(GNUInstallDirs)

install(TARGETS cpp_opengl_app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT CppOpenGLApp_Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT CppOpenGLApp_Runtime
        NAMELINK_COMPONENT CppOpenGLApp_Development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT CppOpenGLApp_Development
)