
/* If compiling on Windows, enabling wide-character
   path support (UTF8) via macros
        STBI_WINDOWS_UTF8 for stb_image.h
        STBIW_WINDOWS_UTF8 for stb_image_write.h
 */

#define STB_IMAGE_IMPLEMENTATION
#if defined(_WIN32)
    #define STBI_WINDOWS_UTF8
#endif
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_WIN32)
    #define STBIW_WINDOWS_UTF8
#endif
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"