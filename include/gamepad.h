#pragma once

#include <cstdint>

namespace Nothofagus
{

// clang-format off
enum class GamepadButton : std::uint8_t
{
    A, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DpadUp, DpadRight, DpadDown, DpadLeft
};

enum class GamepadAxis : std::uint8_t
{
    LeftX, LeftY, RightX, RightY,
    LeftTrigger, RightTrigger
};
// clang-format on

namespace GamepadImplementation
{
    using InternalButtonCode = int;
    using InternalAxisCode   = int;

    InternalButtonCode toInternalButtonCode(GamepadButton button);
    GamepadButton      toGamepadButton(InternalButtonCode code);

    InternalAxisCode   toInternalAxisCode(GamepadAxis axis);
    GamepadAxis        toGamepadAxis(InternalAxisCode code);
}

}
