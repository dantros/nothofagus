
#include "mouse.h"
#include <GLFW/glfw3.h>

namespace Nothofagus
{

namespace MouseImplementation
{

MouseButton toMouseButton(int glfwButton)
{
    switch (glfwButton)
    {
        case GLFW_MOUSE_BUTTON_LEFT:   return MouseButton::Left;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
        case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
        default:                       return MouseButton::Left;
    }
}

int toGLFWMouseButton(MouseButton button)
{
    switch (button)
    {
        case MouseButton::Left:   return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
        case MouseButton::Right:  return GLFW_MOUSE_BUTTON_RIGHT;
        default:                  return GLFW_MOUSE_BUTTON_LEFT;
    }
}

} // namespace MouseImplementation

} // namespace Nothofagus
