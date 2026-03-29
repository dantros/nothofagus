#pragma once

#include "mouse.h"

namespace Nothofagus
{

struct GlfwMouseBackend
{
    static MouseButton toMouseButton(int button);
    static int toInternalMouseButton(MouseButton button);
};

} // namespace Nothofagus
