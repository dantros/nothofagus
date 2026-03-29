#include "glfw_mouse.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Nothofagus
{

MouseButton GlfwMouseBackend::toMouseButton(int button)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:   return MouseButton::Left;
    case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
    case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
    default:                       return MouseButton::Left;
    }
}

int GlfwMouseBackend::toInternalMouseButton(MouseButton button)
{
    switch (button)
    {
    case MouseButton::Left:   return GLFW_MOUSE_BUTTON_LEFT;
    case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
    case MouseButton::Right:  return GLFW_MOUSE_BUTTON_RIGHT;
    default:                  return GLFW_MOUSE_BUTTON_LEFT;
    }
}

} // namespace Nothofagus
