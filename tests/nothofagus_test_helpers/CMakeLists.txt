# Shared test helpers: DirectTexture image I/O (direct_texture_io, via
# stb_image_plus) and tolerance-based comparison (direct_texture_compare).
# Linked by both the visual test executable and the Visual Tests Explorer tool, so
# the file I/O that Nothofagus itself deliberately avoids lives here, in one place.

# Bring in stb_image_plus once (it may also be pulled in by the viewer tool).
if(NOT TARGET stb_image_plus)
    set(STB_IMAGE_PLUS_INSTALL    OFF CACHE BOOL "" FORCE)
    set(STB_IMAGE_PLUS_BUILD_DEMO OFF CACHE BOOL "" FORCE)
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/third_party/stb_image_plus"
        "${CMAKE_BINARY_DIR}/third_party/stb_image_plus"
    )
endif()

add_library(nothofagus_test_helpers STATIC
    "direct_texture_io.cpp"
    "direct_texture_io.h"
    "direct_texture_compare.cpp"
    "direct_texture_compare.h"
)
set_property(TARGET nothofagus_test_helpers PROPERTY CXX_STANDARD 20)
target_include_directories(nothofagus_test_helpers PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(nothofagus_test_helpers PUBLIC nothofagus stb_image_plus)
