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

file(GLOB_RECURSE HEADERS "source/*.h" "include/*.h")

add_library(nothofagus
    "source/canvas.cpp"
    "source/canvas_impl.cpp"
    "source/bellota_to_mesh.cpp"
    "source/mesh.cpp"
    "source/dmesh.cpp"
    "source/transform.cpp"
    "source/texture.cpp"
    "source/performance_monitor.cpp"
    "source/controller.cpp"
    "source/keyboard.cpp"

    # This is only required to show header files in the Visual Studio IDE solution browser
    ${HEADERS}
)
set_property(TARGET nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(nothofagus PRIVATE
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
target_link_libraries(nothofagus PRIVATE whereami2cpp glad glfw imgui)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(NOTHOFAGUS_INCLUDE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
        PARENT_SCOPE
    )
else()
    set(NOTHOFAGUS_INCLUDE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
    )
endif()


option(NOTHOFAGUS_INSTALL "Do you whish to install Nothofagus static library?" ON)
if (${NOTHOFAGUS_INSTALL})
    include(GNUInstallDirs)
    install(TARGETS nothofagus
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT Nothofagus_Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Nothofagus_Runtime
            NAMELINK_COMPONENT Nothofagus_Development
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Nothofagus_Development
    )

    option(NOTHOFAGUS_BUILD_EXAMPLES "Build Nothofagus examples?" OFF)
    if (${NOTHOFAGUS_BUILD_EXAMPLES})
        add_subdirectory("examples")

        if (${NOTHOFAGUS_BUILD_EXAMPLES})
            install(TARGETS hello_nothofagus test_keyboard
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                    COMPONENT NothofagusExamples_Runtime
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    COMPONENT NothofagusExamples_Runtime
                    NAMELINK_COMPONENT NothofagusExamples_Development
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    COMPONENT NothofagusExamples_Development
            )
        endif()
    endif()
endif()