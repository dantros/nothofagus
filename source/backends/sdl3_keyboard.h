#pragma once

#include "keyboard.h"

namespace Nothofagus
{

struct Sdl3KeyboardBackend
{
    static KeyboardImplementation::InternalKeyCode toInternalKeyCode(Key key);
    static Key toKeyCode(KeyboardImplementation::InternalKeyCode code);
};

} // namespace Nothofagus
