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

GamepadImplementation::InternalButtonCode GlfwGamepadBackend::toInternalButtonCode(GamepadButton button)
{
    return static_cast<int>(button);
}

GamepadButton GlfwGamepadBackend::toGamepadButton(GamepadImplementation::InternalButtonCode code)
{
    return static_cast<GamepadButton>(code);
}

GamepadImplementation::InternalAxisCode GlfwGamepadBackend::toInternalAxisCode(GamepadAxis axis)
{
    return static_cast<int>(axis);
}

GamepadAxis GlfwGamepadBackend::toGamepadAxis(GamepadImplementation::InternalAxisCode code)
{
    return static_cast<GamepadAxis>(code);
}

} // namespace Nothofagus
