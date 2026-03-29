#pragma once

#include <cstdint>

namespace Nothofagus
{

enum class MouseButton : std::uint8_t { Left, Middle, Right };

namespace MouseImplementation
{
MouseButton toMouseButton(int button);
int toInternalMouseButton(MouseButton button);
}

}