#include "keyboard.h"
#include "backends/keyboard_backend.h"

namespace Nothofagus::KeyboardImplementation
{

InternalKeyCode toInternalKeyCode(Key key)
{
    return SelectedKeyboardBackend::toInternalKeyCode(key);
}

Key toKeyCode(InternalKeyCode code)
{
    return SelectedKeyboardBackend::toKeyCode(code);
}

} // namespace Nothofagus::KeyboardImplementation
