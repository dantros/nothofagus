# Shared golden-image helper: PNG save/load (via stb_image_plus), tolerance-based
# comparison, and visual diff generation. Linked by both the visual test
# executable and the golden-image viewer tool, so the file I/O that Nothofagus
# itself deliberately avoids lives here, in one place.

# Bring in stb_image_plus once (it may also be pulled in by the viewer tool).
if(NOT TARGET stb_image_plus)
    set(STB_IMAGE_PLUS_INSTALL    OFF CACHE BOOL "" FORCE)
    set(STB_IMAGE_PLUS_BUILD_DEMO OFF CACHE BOOL "" FORCE)
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/third_party/stb_image_plus"
        "${CMAKE_BINARY_DIR}/third_party/stb_image_plus"
    )
endif()

add_library(nothofagus_golden STATIC
    "golden_image.cpp"
    "golden_image.h"
)
set_property(TARGET nothofagus_golden PROPERTY CXX_STANDARD 20)
target_include_directories(nothofagus_golden PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(nothofagus_golden PUBLIC nothofagus stb_image_plus)
