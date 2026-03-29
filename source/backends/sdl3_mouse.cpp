#include "sdl3_mouse.h"
#include <SDL3/SDL.h>

namespace Nothofagus
{

MouseButton Sdl3MouseBackend::toMouseButton(int button)
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:   return MouseButton::Left;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    case SDL_BUTTON_RIGHT:  return MouseButton::Right;
    default:                return MouseButton::Left;
    }
}

int Sdl3MouseBackend::toInternalMouseButton(MouseButton button)
{
    switch (button)
    {
    case MouseButton::Left:   return SDL_BUTTON_LEFT;
    case MouseButton::Middle: return SDL_BUTTON_MIDDLE;
    case MouseButton::Right:  return SDL_BUTTON_RIGHT;
    default:                  return SDL_BUTTON_LEFT;
    }
}

} // namespace Nothofagus
