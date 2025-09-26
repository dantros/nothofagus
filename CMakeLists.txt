cmake_minimum_required(VERSION 3.26)

project(nothofagus_project CXX)

# Setting up build options for GLFW
option(GLFW_INSTALL "Disabling installation of GLFW" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
add_subdirectory("third_party/glfw")

add_subdirectory("third_party/glad")
add_subdirectory("third_party/imgui_cmake")

file(GLOB_RECURSE NOTHOFAGUS_SOURCE_HEADERS "source/*.h")
file(GLOB_RECURSE NOTHOFAGUS_PUBLIC_HEADERS "include/*.h")

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
    "source/text.cpp"
    "source/animation_state_machine.cpp"
    "source/animation_state.cpp"

    # This is only required to show header files in the Visual Studio IDE solution browser
    ${NOTHOFAGUS_SOURCE_HEADERS}
    ${NOTHOFAGUS_PUBLIC_HEADERS}
)
set_property(TARGET nothofagus PROPERTY CXX_STANDARD 20)
target_include_directories(nothofagus PRIVATE
    "include"
    "third_party/spdlog/include"
    "third_party/glad/include"
    "third_party/glfw/include"
    "third_party/glm"
    "third_party/imgui"
    "third_party/font8x8"
)
target_link_libraries(nothofagus PRIVATE glad glfw imgui)
# spdlog uses fmt to format string, this library requires /utf-8 when using MSVC
# The issue has not been fixed. More info at: https://github.com/gabime/spdlog/issues/3251
# diabling FMT_UNICODE is a workaround.
if(MSVC)
    target_compile_definitions(nothofagus PRIVATE FMT_UNICODE=0)
endif()

set(NOTHOFAGUS_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include"
)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(NOTHOFAGUS_INCLUDE ${NOTHOFAGUS_HEADERS} PARENT_SCOPE)
else()
    set(NOTHOFAGUS_INCLUDE ${NOTHOFAGUS_HEADERS})
endif()

option(NOTHOFAGUS_BUILD_EXAMPLES "Build Nothofagus examples?" OFF)
if (${NOTHOFAGUS_BUILD_EXAMPLES})
    add_subdirectory("examples")
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
            hello_animation_state_machine0
            hello_animation_state_machine
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
