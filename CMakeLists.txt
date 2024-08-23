cmake_minimum_required(VERSION 3.20)

project(nothofagus_project CXX)

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

file(GLOB_RECURSE HEADERS "source/*.h")

add_executable(hello_nothofagus
    "examples/hello_nothofagus.cpp"
    "source/canvas.cpp"
    "source/bellota_to_mesh.cpp"
    "source/mesh.cpp"
    "source/dmesh.cpp"

    # This is only required to show header files in the Visual Studio IDE solution browser
    ${HEADERS}
)
set_property(TARGET hello_nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(hello_nothofagus PRIVATE
    "include"
    "third_party/argparse/include"
    "third_party/whereami2cpp/source"
    "third_party/spdlog/include"
    "third_party/json/include"
    "third_party/glad/include"
    "third_party/glfw/include"
    "third_party/glm"
    "third_party/imgui"
)
target_link_libraries(hello_nothofagus PRIVATE whereami2cpp glad glfw imgui)

include(GNUInstallDirs)
install(TARGETS hello_nothofagus
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT HelloNothofagus_Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT HelloNothofagus_Runtime
        NAMELINK_COMPONENT HelloNothofagus_Development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT HelloNothofagus_Development
)