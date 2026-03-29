#pragma once

#include "keyboard.h"

namespace Nothofagus
{

struct GlfwKeyboardBackend
{
    static KeyboardImplementation::InternalKeyCode toInternalKeyCode(Key key);
    static Key toKeyCode(KeyboardImplementation::InternalKeyCode code);
};

} // namespace Nothofagus
