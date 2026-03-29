#include "mouse.h"
#include "backends/mouse_backend.h"

namespace Nothofagus::MouseImplementation
{

MouseButton toMouseButton(int button)
{
    return SelectedMouseBackend::toMouseButton(button);
}

int toInternalMouseButton(MouseButton button)
{
    return SelectedMouseBackend::toInternalMouseButton(button);
}

} // namespace Nothofagus::MouseImplementation
