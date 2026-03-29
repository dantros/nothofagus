#include "sdl3_keyboard.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace Nothofagus
{

// clang-format off
KeyboardImplementation::InternalKeyCode Sdl3KeyboardBackend::toInternalKeyCode(Key key)
{
    switch (key)
    {
    // Letters
    case Key::A: return SDL_SCANCODE_A;
    case Key::B: return SDL_SCANCODE_B;
    case Key::C: return SDL_SCANCODE_C;
    case Key::D: return SDL_SCANCODE_D;
    case Key::E: return SDL_SCANCODE_E;
    case Key::F: return SDL_SCANCODE_F;
    case Key::G: return SDL_SCANCODE_G;
    case Key::H: return SDL_SCANCODE_H;
    case Key::I: return SDL_SCANCODE_I;
    case Key::J: return SDL_SCANCODE_J;
    case Key::K: return SDL_SCANCODE_K;
    case Key::L: return SDL_SCANCODE_L;
    case Key::M: return SDL_SCANCODE_M;
    case Key::N: return SDL_SCANCODE_N;
    case Key::O: return SDL_SCANCODE_O;
    case Key::P: return SDL_SCANCODE_P;
    case Key::Q: return SDL_SCANCODE_Q;
    case Key::R: return SDL_SCANCODE_R;
    case Key::S: return SDL_SCANCODE_S;
    case Key::T: return SDL_SCANCODE_T;
    case Key::U: return SDL_SCANCODE_U;
    case Key::V: return SDL_SCANCODE_V;
    case Key::W: return SDL_SCANCODE_W;
    case Key::X: return SDL_SCANCODE_X;
    case Key::Y: return SDL_SCANCODE_Y;
    case Key::Z: return SDL_SCANCODE_Z;
    // Numbers
    case Key::_0: return SDL_SCANCODE_0;
    case Key::_1: return SDL_SCANCODE_1;
    case Key::_2: return SDL_SCANCODE_2;
    case Key::_3: return SDL_SCANCODE_3;
    case Key::_4: return SDL_SCANCODE_4;
    case Key::_5: return SDL_SCANCODE_5;
    case Key::_6: return SDL_SCANCODE_6;
    case Key::_7: return SDL_SCANCODE_7;
    case Key::_8: return SDL_SCANCODE_8;
    case Key::_9: return SDL_SCANCODE_9;
    // Punctuation
    case Key::APOSTROPHE:    return SDL_SCANCODE_APOSTROPHE;
    case Key::COMMA:         return SDL_SCANCODE_COMMA;
    case Key::MINUS:         return SDL_SCANCODE_MINUS;
    case Key::PERIOD:        return SDL_SCANCODE_PERIOD;
    case Key::SLASH:         return SDL_SCANCODE_SLASH;
    case Key::SEMICOLON:     return SDL_SCANCODE_SEMICOLON;
    case Key::EQUAL:         return SDL_SCANCODE_EQUALS;
    case Key::LEFT_BRACKET:  return SDL_SCANCODE_LEFTBRACKET;
    case Key::BACKSLASH:     return SDL_SCANCODE_BACKSLASH;
    case Key::RIGHT_BRACKET: return SDL_SCANCODE_RIGHTBRACKET;
    case Key::GRAVE_ACCENT:  return SDL_SCANCODE_GRAVE;
    case Key::WORLD_1:       return SDL_SCANCODE_UNKNOWN;
    case Key::WORLD_2:       return SDL_SCANCODE_UNKNOWN;
    // Navigation / editing
    case Key::LEFT:      return SDL_SCANCODE_LEFT;
    case Key::RIGHT:     return SDL_SCANCODE_RIGHT;
    case Key::UP:        return SDL_SCANCODE_UP;
    case Key::DOWN:      return SDL_SCANCODE_DOWN;
    case Key::PAGE_UP:   return SDL_SCANCODE_PAGEUP;
    case Key::PAGE_DOWN: return SDL_SCANCODE_PAGEDOWN;
    case Key::HOME:      return SDL_SCANCODE_HOME;
    case Key::END:       return SDL_SCANCODE_END;
    case Key::INSERT:    return SDL_SCANCODE_INSERT;
    case Key::DEL:       return SDL_SCANCODE_DELETE;
    case Key::BACKSPACE: return SDL_SCANCODE_BACKSPACE;
    case Key::TAB:       return SDL_SCANCODE_TAB;
    // Control
    case Key::SPACE:        return SDL_SCANCODE_SPACE;
    case Key::ESCAPE:       return SDL_SCANCODE_ESCAPE;
    case Key::ENTER:        return SDL_SCANCODE_RETURN;
    case Key::CAPS_LOCK:    return SDL_SCANCODE_CAPSLOCK;
    case Key::SCROLL_LOCK:  return SDL_SCANCODE_SCROLLLOCK;
    case Key::NUM_LOCK:     return SDL_SCANCODE_NUMLOCKCLEAR;
    case Key::PRINT_SCREEN: return SDL_SCANCODE_PRINTSCREEN;
    case Key::PAUSE:        return SDL_SCANCODE_PAUSE;
    // Function keys
    case Key::F1:  return SDL_SCANCODE_F1;
    case Key::F2:  return SDL_SCANCODE_F2;
    case Key::F3:  return SDL_SCANCODE_F3;
    case Key::F4:  return SDL_SCANCODE_F4;
    case Key::F5:  return SDL_SCANCODE_F5;
    case Key::F6:  return SDL_SCANCODE_F6;
    case Key::F7:  return SDL_SCANCODE_F7;
    case Key::F8:  return SDL_SCANCODE_F8;
    case Key::F9:  return SDL_SCANCODE_F9;
    case Key::F10: return SDL_SCANCODE_F10;
    case Key::F11: return SDL_SCANCODE_F11;
    case Key::F12: return SDL_SCANCODE_F12;
    case Key::F13: return SDL_SCANCODE_F13;
    case Key::F14: return SDL_SCANCODE_F14;
    case Key::F15: return SDL_SCANCODE_F15;
    case Key::F16: return SDL_SCANCODE_F16;
    case Key::F17: return SDL_SCANCODE_F17;
    case Key::F18: return SDL_SCANCODE_F18;
    case Key::F19: return SDL_SCANCODE_F19;
    case Key::F20: return SDL_SCANCODE_F20;
    case Key::F21: return SDL_SCANCODE_F21;
    case Key::F22: return SDL_SCANCODE_F22;
    case Key::F23: return SDL_SCANCODE_F23;
    case Key::F24: return SDL_SCANCODE_F24;
    case Key::F25: return SDL_SCANCODE_UNKNOWN; // SDL3 only goes to F24
    // Keypad
    case Key::KP_0:        return SDL_SCANCODE_KP_0;
    case Key::KP_1:        return SDL_SCANCODE_KP_1;
    case Key::KP_2:        return SDL_SCANCODE_KP_2;
    case Key::KP_3:        return SDL_SCANCODE_KP_3;
    case Key::KP_4:        return SDL_SCANCODE_KP_4;
    case Key::KP_5:        return SDL_SCANCODE_KP_5;
    case Key::KP_6:        return SDL_SCANCODE_KP_6;
    case Key::KP_7:        return SDL_SCANCODE_KP_7;
    case Key::KP_8:        return SDL_SCANCODE_KP_8;
    case Key::KP_9:        return SDL_SCANCODE_KP_9;
    case Key::KP_DECIMAL:  return SDL_SCANCODE_KP_PERIOD;
    case Key::KP_DIVIDE:   return SDL_SCANCODE_KP_DIVIDE;
    case Key::KP_MULTIPLY: return SDL_SCANCODE_KP_MULTIPLY;
    case Key::KP_SUBTRACT: return SDL_SCANCODE_KP_MINUS;
    case Key::KP_ADD:      return SDL_SCANCODE_KP_PLUS;
    case Key::KP_ENTER:    return SDL_SCANCODE_KP_ENTER;
    case Key::KP_EQUAL:    return SDL_SCANCODE_KP_EQUALS;
    // Modifiers
    case Key::LEFT_SHIFT:    return SDL_SCANCODE_LSHIFT;
    case Key::LEFT_CONTROL:  return SDL_SCANCODE_LCTRL;
    case Key::LEFT_ALT:      return SDL_SCANCODE_LALT;
    case Key::LEFT_SUPER:    return SDL_SCANCODE_LGUI;
    case Key::RIGHT_SHIFT:   return SDL_SCANCODE_RSHIFT;
    case Key::RIGHT_CONTROL: return SDL_SCANCODE_RCTRL;
    case Key::RIGHT_ALT:     return SDL_SCANCODE_RALT;
    case Key::RIGHT_SUPER:   return SDL_SCANCODE_RGUI;
    case Key::MENU:          return SDL_SCANCODE_MENU;
    default:
        spdlog::error("Undefined Key {}", static_cast<int>(key));
        return SDL_SCANCODE_UNKNOWN;
    };
}

Key Sdl3KeyboardBackend::toKeyCode(KeyboardImplementation::InternalKeyCode code)
{
    switch (code)
    {
    // Letters
    case SDL_SCANCODE_A: return Key::A;
    case SDL_SCANCODE_B: return Key::B;
    case SDL_SCANCODE_C: return Key::C;
    case SDL_SCANCODE_D: return Key::D;
    case SDL_SCANCODE_E: return Key::E;
    case SDL_SCANCODE_F: return Key::F;
    case SDL_SCANCODE_G: return Key::G;
    case SDL_SCANCODE_H: return Key::H;
    case SDL_SCANCODE_I: return Key::I;
    case SDL_SCANCODE_J: return Key::J;
    case SDL_SCANCODE_K: return Key::K;
    case SDL_SCANCODE_L: return Key::L;
    case SDL_SCANCODE_M: return Key::M;
    case SDL_SCANCODE_N: return Key::N;
    case SDL_SCANCODE_O: return Key::O;
    case SDL_SCANCODE_P: return Key::P;
    case SDL_SCANCODE_Q: return Key::Q;
    case SDL_SCANCODE_R: return Key::R;
    case SDL_SCANCODE_S: return Key::S;
    case SDL_SCANCODE_T: return Key::T;
    case SDL_SCANCODE_U: return Key::U;
    case SDL_SCANCODE_V: return Key::V;
    case SDL_SCANCODE_W: return Key::W;
    case SDL_SCANCODE_X: return Key::X;
    case SDL_SCANCODE_Y: return Key::Y;
    case SDL_SCANCODE_Z: return Key::Z;
    // Numbers
    case SDL_SCANCODE_0: return Key::_0;
    case SDL_SCANCODE_1: return Key::_1;
    case SDL_SCANCODE_2: return Key::_2;
    case SDL_SCANCODE_3: return Key::_3;
    case SDL_SCANCODE_4: return Key::_4;
    case SDL_SCANCODE_5: return Key::_5;
    case SDL_SCANCODE_6: return Key::_6;
    case SDL_SCANCODE_7: return Key::_7;
    case SDL_SCANCODE_8: return Key::_8;
    case SDL_SCANCODE_9: return Key::_9;
    // Punctuation
    case SDL_SCANCODE_APOSTROPHE:   return Key::APOSTROPHE;
    case SDL_SCANCODE_COMMA:        return Key::COMMA;
    case SDL_SCANCODE_MINUS:        return Key::MINUS;
    case SDL_SCANCODE_PERIOD:       return Key::PERIOD;
    case SDL_SCANCODE_SLASH:        return Key::SLASH;
    case SDL_SCANCODE_SEMICOLON:    return Key::SEMICOLON;
    case SDL_SCANCODE_EQUALS:       return Key::EQUAL;
    case SDL_SCANCODE_LEFTBRACKET:  return Key::LEFT_BRACKET;
    case SDL_SCANCODE_BACKSLASH:    return Key::BACKSLASH;
    case SDL_SCANCODE_RIGHTBRACKET: return Key::RIGHT_BRACKET;
    case SDL_SCANCODE_GRAVE:        return Key::GRAVE_ACCENT;
    // Navigation / editing
    case SDL_SCANCODE_LEFT:      return Key::LEFT;
    case SDL_SCANCODE_RIGHT:     return Key::RIGHT;
    case SDL_SCANCODE_UP:        return Key::UP;
    case SDL_SCANCODE_DOWN:      return Key::DOWN;
    case SDL_SCANCODE_PAGEUP:    return Key::PAGE_UP;
    case SDL_SCANCODE_PAGEDOWN:  return Key::PAGE_DOWN;
    case SDL_SCANCODE_HOME:      return Key::HOME;
    case SDL_SCANCODE_END:       return Key::END;
    case SDL_SCANCODE_INSERT:    return Key::INSERT;
    case SDL_SCANCODE_DELETE:    return Key::DEL;
    case SDL_SCANCODE_BACKSPACE: return Key::BACKSPACE;
    case SDL_SCANCODE_TAB:       return Key::TAB;
    // Control
    case SDL_SCANCODE_SPACE:        return Key::SPACE;
    case SDL_SCANCODE_ESCAPE:       return Key::ESCAPE;
    case SDL_SCANCODE_RETURN:       return Key::ENTER;
    case SDL_SCANCODE_CAPSLOCK:     return Key::CAPS_LOCK;
    case SDL_SCANCODE_SCROLLLOCK:   return Key::SCROLL_LOCK;
    case SDL_SCANCODE_NUMLOCKCLEAR: return Key::NUM_LOCK;
    case SDL_SCANCODE_PRINTSCREEN:  return Key::PRINT_SCREEN;
    case SDL_SCANCODE_PAUSE:        return Key::PAUSE;
    // Function keys
    case SDL_SCANCODE_F1:  return Key::F1;
    case SDL_SCANCODE_F2:  return Key::F2;
    case SDL_SCANCODE_F3:  return Key::F3;
    case SDL_SCANCODE_F4:  return Key::F4;
    case SDL_SCANCODE_F5:  return Key::F5;
    case SDL_SCANCODE_F6:  return Key::F6;
    case SDL_SCANCODE_F7:  return Key::F7;
    case SDL_SCANCODE_F8:  return Key::F8;
    case SDL_SCANCODE_F9:  return Key::F9;
    case SDL_SCANCODE_F10: return Key::F10;
    case SDL_SCANCODE_F11: return Key::F11;
    case SDL_SCANCODE_F12: return Key::F12;
    case SDL_SCANCODE_F13: return Key::F13;
    case SDL_SCANCODE_F14: return Key::F14;
    case SDL_SCANCODE_F15: return Key::F15;
    case SDL_SCANCODE_F16: return Key::F16;
    case SDL_SCANCODE_F17: return Key::F17;
    case SDL_SCANCODE_F18: return Key::F18;
    case SDL_SCANCODE_F19: return Key::F19;
    case SDL_SCANCODE_F20: return Key::F20;
    case SDL_SCANCODE_F21: return Key::F21;
    case SDL_SCANCODE_F22: return Key::F22;
    case SDL_SCANCODE_F23: return Key::F23;
    case SDL_SCANCODE_F24: return Key::F24;
    // Keypad
    case SDL_SCANCODE_KP_0:        return Key::KP_0;
    case SDL_SCANCODE_KP_1:        return Key::KP_1;
    case SDL_SCANCODE_KP_2:        return Key::KP_2;
    case SDL_SCANCODE_KP_3:        return Key::KP_3;
    case SDL_SCANCODE_KP_4:        return Key::KP_4;
    case SDL_SCANCODE_KP_5:        return Key::KP_5;
    case SDL_SCANCODE_KP_6:        return Key::KP_6;
    case SDL_SCANCODE_KP_7:        return Key::KP_7;
    case SDL_SCANCODE_KP_8:        return Key::KP_8;
    case SDL_SCANCODE_KP_9:        return Key::KP_9;
    case SDL_SCANCODE_KP_PERIOD:   return Key::KP_DECIMAL;
    case SDL_SCANCODE_KP_DIVIDE:   return Key::KP_DIVIDE;
    case SDL_SCANCODE_KP_MULTIPLY: return Key::KP_MULTIPLY;
    case SDL_SCANCODE_KP_MINUS:    return Key::KP_SUBTRACT;
    case SDL_SCANCODE_KP_PLUS:     return Key::KP_ADD;
    case SDL_SCANCODE_KP_ENTER:    return Key::KP_ENTER;
    case SDL_SCANCODE_KP_EQUALS:   return Key::KP_EQUAL;
    // Modifiers
    case SDL_SCANCODE_LSHIFT: return Key::LEFT_SHIFT;
    case SDL_SCANCODE_LCTRL:  return Key::LEFT_CONTROL;
    case SDL_SCANCODE_LALT:   return Key::LEFT_ALT;
    case SDL_SCANCODE_LGUI:   return Key::LEFT_SUPER;
    case SDL_SCANCODE_RSHIFT: return Key::RIGHT_SHIFT;
    case SDL_SCANCODE_RCTRL:  return Key::RIGHT_CONTROL;
    case SDL_SCANCODE_RALT:   return Key::RIGHT_ALT;
    case SDL_SCANCODE_RGUI:   return Key::RIGHT_SUPER;
    case SDL_SCANCODE_MENU:   return Key::MENU;
    default:
        spdlog::error("Undefined Key {}", static_cast<int>(code));
        return Key::SIZEOF;
    };
}
// clang-format on

} // namespace Nothofagus
