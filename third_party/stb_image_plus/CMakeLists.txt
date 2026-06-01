cmake_minimum_required(VERSION 3.20)

project(stb_image_proj CXX)

add_library(stb_image_orig STATIC
    "stb_image/stb_image.cpp"
    "stb_image/stb_image.h"
    "stb_image/stb_image_resize2.h"
    "stb_image/stb_image_write.h"
)
set_property(TARGET stb_image_orig PROPERTY CXX_STANDARD 20)

set(STB_IMAGE_PLUS_PUBLIC_HEADERS
    "include/stb_image_plus.h"
    "include/stb_image_plus_gif.h"
)

add_library(stb_image_plus STATIC
    "source/stb_image_plus.cpp"
    "source/stb_image_plus_gif.cpp"
    ${STB_IMAGE_PLUS_PUBLIC_HEADERS}
)
target_include_directories(stb_image_plus
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/stb_image"
)
target_link_libraries(stb_image_plus PRIVATE stb_image_orig)
set_property(TARGET stb_image_plus PROPERTY CXX_STANDARD 20)

option(STB_IMAGE_PLUS_BUILD_DEMO "" OFF)
if(${STB_IMAGE_PLUS_BUILD_DEMO})
    add_executable(resize_demo "demo/resize_demo.cpp")
    target_include_directories(resize_demo PRIVATE "include")
    target_link_libraries(resize_demo PRIVATE stb_image_plus)
    set_property(TARGET resize_demo PROPERTY CXX_STANDARD 20)
endif()

option(STB_IMAGE_PLUS_INSTALL "" OFF)
if(${STB_IMAGE_PLUS_INSTALL})

    # Installing static library
    include(GNUInstallDirs)
    install(TARGETS stb_image_plus
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT StbImagePlus_Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT StbImagePlus_Runtime
            NAMELINK_COMPONENT StbImagePlus_Development
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT StbImagePlus_Development
    )

    # Installing public headers
    target_sources(stb_image_plus PUBLIC
        FILE_SET stb_image_plus_Headers
        TYPE HEADERS
        BASE_DIRS include
        FILES ${STB_IMAGE_PLUS_PUBLIC_HEADERS}
    )
    install(TARGETS stb_image_plus FILE_SET stb_image_plus_Headers)

    # Installing demo if built
    if(${STB_IMAGE_PLUS_BUILD_DEMO})
        install(TARGETS resize_demo
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT StbImagePlus_Runtime
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT StbImagePlus_Runtime
                NAMELINK_COMPONENT StbImagePlus_Development
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT StbImagePlus_Development
        )
    endif()
endif()