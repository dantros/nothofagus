#pragma once

#include "mouse.h"
#include <concepts>

#ifdef NOTHOFAGUS_BACKEND_SDL3
  #include "sdl3_mouse.h"
#else
  #include "glfw_mouse.h"
#endif

namespace Nothofagus
{

template<typename T>
concept MouseBackend = requires(MouseButton button, int code)
{
    { T::toMouseButton(code)           } -> std::same_as<MouseButton>;
    { T::toInternalMouseButton(button) } -> std::same_as<int>;
};

#ifdef NOTHOFAGUS_BACKEND_SDL3
  using SelectedMouseBackend = Sdl3MouseBackend;
#else
  using SelectedMouseBackend = GlfwMouseBackend;
#endif

static_assert(MouseBackend<SelectedMouseBackend>,
    "Selected mouse backend does not satisfy the MouseBackend concept");

} // namespace Nothofagus
