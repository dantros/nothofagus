#include "gamepad.h"
#include <concepts>

#ifdef NOTHOFAGUS_BACKEND_SDL3
  #include "backends/sdl3_gamepad.h"
#else
  #include "backends/glfw_gamepad.h"
#endif

namespace Nothofagus
{

template<typename T>
concept GamepadBackend = requires(GamepadButton button, GamepadAxis axis,
                                   GamepadImplementation::InternalButtonCode buttonCode,
                                   GamepadImplementation::InternalAxisCode axisCode)
{
    { T::toInternalButtonCode(button)   } -> std::same_as<GamepadImplementation::InternalButtonCode>;
    { T::toGamepadButton(buttonCode)    } -> std::same_as<GamepadButton>;
    { T::toInternalAxisCode(axis)       } -> std::same_as<GamepadImplementation::InternalAxisCode>;
    { T::toGamepadAxis(axisCode)        } -> std::same_as<GamepadAxis>;
};

#ifdef NOTHOFAGUS_BACKEND_SDL3
  using SelectedGamepadBackend = Sdl3GamepadBackend;
#else
  using SelectedGamepadBackend = GlfwGamepadBackend;
#endif

static_assert(GamepadBackend<SelectedGamepadBackend>,
    "Selected gamepad backend does not satisfy the GamepadBackend concept");

namespace GamepadImplementation
{

InternalButtonCode toInternalButtonCode(GamepadButton button)
{
    return SelectedGamepadBackend::toInternalButtonCode(button);
}

GamepadButton toGamepadButton(InternalButtonCode code)
{
    return SelectedGamepadBackend::toGamepadButton(code);
}

InternalAxisCode toInternalAxisCode(GamepadAxis axis)
{
    return SelectedGamepadBackend::toInternalAxisCode(axis);
}

GamepadAxis toGamepadAxis(InternalAxisCode code)
{
    return SelectedGamepadBackend::toGamepadAxis(code);
}

} // namespace GamepadImplementation

} // namespace Nothofagus
