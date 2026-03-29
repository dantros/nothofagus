#pragma once

#include "keyboard.h"
#include <concepts>

#ifdef NOTHOFAGUS_BACKEND_SDL3
  #include "sdl3_keyboard.h"
#else
  #include "glfw_keyboard.h"
#endif

namespace Nothofagus
{

template<typename T>
concept KeyboardBackend = requires(Key key, KeyboardImplementation::InternalKeyCode code)
{
    { T::toInternalKeyCode(key) } -> std::same_as<KeyboardImplementation::InternalKeyCode>;
    { T::toKeyCode(code)        } -> std::same_as<Key>;
};

#ifdef NOTHOFAGUS_BACKEND_SDL3
  using SelectedKeyboardBackend = Sdl3KeyboardBackend;
#else
  using SelectedKeyboardBackend = GlfwKeyboardBackend;
#endif

static_assert(KeyboardBackend<SelectedKeyboardBackend>,
    "Selected keyboard backend does not satisfy the KeyboardBackend concept");

} // namespace Nothofagus
