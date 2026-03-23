#pragma once

#include <cstdint>

namespace Nothofagus
{

enum class MouseButton : std::uint8_t { Left, Middle, Right };

namespace MouseImplementation
{
MouseButton toMouseButton(int glfwButton);
int toGLFWMouseButton(MouseButton button);
}

}