#include "sdl3_gamepad.h"
#include <SDL3/SDL_gamepad.h>

namespace Nothofagus
{

GamepadImplementation::InternalButtonCode Sdl3GamepadBackend::toInternalButtonCode(GamepadButton button)
{
    switch (button)
    {
    case GamepadButton::A:           return SDL_GAMEPAD_BUTTON_SOUTH;
    case GamepadButton::B:           return SDL_GAMEPAD_BUTTON_EAST;
    case GamepadButton::X:           return SDL_GAMEPAD_BUTTON_WEST;
    case GamepadButton::Y:           return SDL_GAMEPAD_BUTTON_NORTH;
    case GamepadButton::LeftBumper:  return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    case GamepadButton::RightBumper: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case GamepadButton::Back:        return SDL_GAMEPAD_BUTTON_BACK;
    case GamepadButton::Start:       return SDL_GAMEPAD_BUTTON_START;
    case GamepadButton::Guide:       return SDL_GAMEPAD_BUTTON_GUIDE;
    case GamepadButton::LeftThumb:   return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    case GamepadButton::RightThumb:  return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    case GamepadButton::DpadUp:      return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case GamepadButton::DpadRight:   return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    case GamepadButton::DpadDown:    return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case GamepadButton::DpadLeft:    return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    default:                         return SDL_GAMEPAD_BUTTON_SOUTH;
    }
}

GamepadButton Sdl3GamepadBackend::toGamepadButton(GamepadImplementation::InternalButtonCode code)
{
    switch (static_cast<SDL_GamepadButton>(code))
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:          return GamepadButton::A;
    case SDL_GAMEPAD_BUTTON_EAST:           return GamepadButton::B;
    case SDL_GAMEPAD_BUTTON_WEST:           return GamepadButton::X;
    case SDL_GAMEPAD_BUTTON_NORTH:          return GamepadButton::Y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return GamepadButton::LeftBumper;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return GamepadButton::RightBumper;
    case SDL_GAMEPAD_BUTTON_BACK:           return GamepadButton::Back;
    case SDL_GAMEPAD_BUTTON_START:          return GamepadButton::Start;
    case SDL_GAMEPAD_BUTTON_GUIDE:          return GamepadButton::Guide;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return GamepadButton::LeftThumb;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return GamepadButton::RightThumb;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:        return GamepadButton::DpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return GamepadButton::DpadRight;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return GamepadButton::DpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return GamepadButton::DpadLeft;
    default:                                return GamepadButton::A;
    }
}

GamepadImplementation::InternalAxisCode Sdl3GamepadBackend::toInternalAxisCode(GamepadAxis axis)
{
    // SDL3 axis enum order matches GamepadAxis order — direct cast is valid.
    return static_cast<int>(axis);
}

GamepadAxis Sdl3GamepadBackend::toGamepadAxis(GamepadImplementation::InternalAxisCode code)
{
    // SDL3 axis enum order matches GamepadAxis order — direct cast is valid.
    return static_cast<GamepadAxis>(code);
}

} // namespace Nothofagus
