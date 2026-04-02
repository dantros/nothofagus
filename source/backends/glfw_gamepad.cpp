#include "glfw_gamepad.h"
#include <GLFW/glfw3.h>

namespace Nothofagus
{

// Verify GLFW gamepad button/axis indices match our enum order.
// If these fail, the identity mapping below is invalid and a switch is needed.
static_assert(GLFW_GAMEPAD_BUTTON_A == 0);
static_assert(GLFW_GAMEPAD_BUTTON_DPAD_LEFT == 14);
static_assert(GLFW_GAMEPAD_AXIS_LEFT_X == 0);
static_assert(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER == 5);

// GLFW gamepad button/axis indices are identity-mapped to our enums, so a
// static_cast suffices. If the static_asserts above ever fail, replace the
// casts with the explicit switch statements commented inside each function.

GamepadImplementation::InternalButtonCode GlfwGamepadBackend::toInternalButtonCode(GamepadButton button)
{
    // switch (button)
    // {
    // case GamepadButton::A:           return GLFW_GAMEPAD_BUTTON_A;
    // case GamepadButton::B:           return GLFW_GAMEPAD_BUTTON_B;
    // case GamepadButton::X:           return GLFW_GAMEPAD_BUTTON_X;
    // case GamepadButton::Y:           return GLFW_GAMEPAD_BUTTON_Y;
    // case GamepadButton::LeftBumper:  return GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
    // case GamepadButton::RightBumper: return GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
    // case GamepadButton::Back:        return GLFW_GAMEPAD_BUTTON_BACK;
    // case GamepadButton::Start:       return GLFW_GAMEPAD_BUTTON_START;
    // case GamepadButton::Guide:       return GLFW_GAMEPAD_BUTTON_GUIDE;
    // case GamepadButton::LeftThumb:   return GLFW_GAMEPAD_BUTTON_LEFT_THUMB;
    // case GamepadButton::RightThumb:  return GLFW_GAMEPAD_BUTTON_RIGHT_THUMB;
    // case GamepadButton::DpadUp:      return GLFW_GAMEPAD_BUTTON_DPAD_UP;
    // case GamepadButton::DpadRight:   return GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
    // case GamepadButton::DpadDown:    return GLFW_GAMEPAD_BUTTON_DPAD_DOWN;
    // case GamepadButton::DpadLeft:    return GLFW_GAMEPAD_BUTTON_DPAD_LEFT;
    // default:                         return GLFW_GAMEPAD_BUTTON_A;
    // }
    return static_cast<int>(button);
}

GamepadButton GlfwGamepadBackend::toGamepadButton(GamepadImplementation::InternalButtonCode code)
{
    // switch (code)
    // {
    // case GLFW_GAMEPAD_BUTTON_A:            return GamepadButton::A;
    // case GLFW_GAMEPAD_BUTTON_B:            return GamepadButton::B;
    // case GLFW_GAMEPAD_BUTTON_X:            return GamepadButton::X;
    // case GLFW_GAMEPAD_BUTTON_Y:            return GamepadButton::Y;
    // case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:  return GamepadButton::LeftBumper;
    // case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return GamepadButton::RightBumper;
    // case GLFW_GAMEPAD_BUTTON_BACK:         return GamepadButton::Back;
    // case GLFW_GAMEPAD_BUTTON_START:        return GamepadButton::Start;
    // case GLFW_GAMEPAD_BUTTON_GUIDE:        return GamepadButton::Guide;
    // case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:   return GamepadButton::LeftThumb;
    // case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:  return GamepadButton::RightThumb;
    // case GLFW_GAMEPAD_BUTTON_DPAD_UP:      return GamepadButton::DpadUp;
    // case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:   return GamepadButton::DpadRight;
    // case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:    return GamepadButton::DpadDown;
    // case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:    return GamepadButton::DpadLeft;
    // default:                               return GamepadButton::A;
    // }
    return static_cast<GamepadButton>(code);
}

GamepadImplementation::InternalAxisCode GlfwGamepadBackend::toInternalAxisCode(GamepadAxis axis)
{
    // switch (axis)
    // {
    // case GamepadAxis::LeftX:         return GLFW_GAMEPAD_AXIS_LEFT_X;
    // case GamepadAxis::LeftY:         return GLFW_GAMEPAD_AXIS_LEFT_Y;
    // case GamepadAxis::RightX:        return GLFW_GAMEPAD_AXIS_RIGHT_X;
    // case GamepadAxis::RightY:        return GLFW_GAMEPAD_AXIS_RIGHT_Y;
    // case GamepadAxis::LeftTrigger:   return GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
    // case GamepadAxis::RightTrigger:  return GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
    // default:                         return GLFW_GAMEPAD_AXIS_LEFT_X;
    // }
    return static_cast<int>(axis);
}

GamepadAxis GlfwGamepadBackend::toGamepadAxis(GamepadImplementation::InternalAxisCode code)
{
    // switch (code)
    // {
    // case GLFW_GAMEPAD_AXIS_LEFT_X:        return GamepadAxis::LeftX;
    // case GLFW_GAMEPAD_AXIS_LEFT_Y:        return GamepadAxis::LeftY;
    // case GLFW_GAMEPAD_AXIS_RIGHT_X:       return GamepadAxis::RightX;
    // case GLFW_GAMEPAD_AXIS_RIGHT_Y:       return GamepadAxis::RightY;
    // case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER:  return GamepadAxis::LeftTrigger;
    // case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxis::RightTrigger;
    // default:                              return GamepadAxis::LeftX;
    // }
    return static_cast<GamepadAxis>(code);
}

} // namespace Nothofagus
