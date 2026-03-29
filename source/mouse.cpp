
#include "mouse.h"

#ifdef NOTHOFAGUS_BACKEND_SDL3
  #include <SDL3/SDL.h>
#else
  #include <GLFW/glfw3.h>
#endif

namespace Nothofagus
{

namespace MouseImplementation
{

MouseButton toMouseButton(int button)
{
    switch (button)
    {
#ifdef NOTHOFAGUS_BACKEND_SDL3
        case SDL_BUTTON_LEFT:   return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT:  return MouseButton::Right;
#else
        case GLFW_MOUSE_BUTTON_LEFT:   return MouseButton::Left;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
        case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
#endif
        default:                       return MouseButton::Left;
    }
}

int toInternalMouseButton(MouseButton button)
{
    switch (button)
    {
#ifdef NOTHOFAGUS_BACKEND_SDL3
        case MouseButton::Left:   return SDL_BUTTON_LEFT;
        case MouseButton::Middle: return SDL_BUTTON_MIDDLE;
        case MouseButton::Right:  return SDL_BUTTON_RIGHT;
#else
        case MouseButton::Left:   return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
        case MouseButton::Right:  return GLFW_MOUSE_BUTTON_RIGHT;
#endif
        default:                  return 0;
    }
}

} // namespace MouseImplementation

} // namespace Nothofagus
