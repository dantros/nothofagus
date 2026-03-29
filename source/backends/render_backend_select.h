#pragma once

#include "render_backend.h"

#if defined(NOTHOFAGUS_BACKEND_VULKAN)
    #include "vulkan_backend.h"
    namespace Nothofagus { using ActiveBackend = VulkanBackend; }
#else
    #include "opengl_backend.h"
    namespace Nothofagus { using ActiveBackend = OpenGLBackend; }
#endif

static_assert(
    Nothofagus::RenderBackend<Nothofagus::ActiveBackend>,
    "Selected render backend does not satisfy the RenderBackend concept");
