#pragma once

#include "mouse.h"

namespace Nothofagus
{

struct Sdl3MouseBackend
{
    static MouseButton toMouseButton(int button);
    static int toInternalMouseButton(MouseButton button);
};

} // namespace Nothofagus
