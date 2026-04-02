#pragma once

#include "gamepad.h"

namespace Nothofagus
{

struct GlfwGamepadBackend
{
    static GamepadImplementation::InternalButtonCode toInternalButtonCode(GamepadButton button);
    static GamepadButton toGamepadButton(GamepadImplementation::InternalButtonCode code);
    static GamepadImplementation::InternalAxisCode toInternalAxisCode(GamepadAxis axis);
    static GamepadAxis toGamepadAxis(GamepadImplementation::InternalAxisCode code);
};

} // namespace Nothofagus
