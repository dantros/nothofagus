
#include "keyboard.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Nothofagus
{

    namespace KeyboardImplementation
    {

        GLFWKeyCode toGLFWKeyCode(Key key)
        {
            // clang-format off
            switch (key)
            {
            // Letters
            case Key::A: return GLFW_KEY_A;
            case Key::B: return GLFW_KEY_B;
            case Key::C: return GLFW_KEY_C;
            case Key::D: return GLFW_KEY_D;
            case Key::E: return GLFW_KEY_E;
            case Key::F: return GLFW_KEY_F;
            case Key::G: return GLFW_KEY_G;
            case Key::H: return GLFW_KEY_H;
            case Key::I: return GLFW_KEY_I;
            case Key::J: return GLFW_KEY_J;
            case Key::K: return GLFW_KEY_K;
            case Key::L: return GLFW_KEY_L;
            case Key::M: return GLFW_KEY_M;
            case Key::N: return GLFW_KEY_N;
            case Key::O: return GLFW_KEY_O;
            case Key::P: return GLFW_KEY_P;
            case Key::Q: return GLFW_KEY_Q;
            case Key::R: return GLFW_KEY_R;
            case Key::S: return GLFW_KEY_S;
            case Key::T: return GLFW_KEY_T;
            case Key::U: return GLFW_KEY_U;
            case Key::V: return GLFW_KEY_V;
            case Key::W: return GLFW_KEY_W;
            case Key::X: return GLFW_KEY_X;
            case Key::Y: return GLFW_KEY_Y;
            case Key::Z: return GLFW_KEY_Z;
            // Numbers
            case Key::_0: return GLFW_KEY_0;
            case Key::_1: return GLFW_KEY_1;
            case Key::_2: return GLFW_KEY_2;
            case Key::_3: return GLFW_KEY_3;
            case Key::_4: return GLFW_KEY_4;
            case Key::_5: return GLFW_KEY_5;
            case Key::_6: return GLFW_KEY_6;
            case Key::_7: return GLFW_KEY_7;
            case Key::_8: return GLFW_KEY_8;
            case Key::_9: return GLFW_KEY_9;
            // Punctuation
            case Key::APOSTROPHE:    return GLFW_KEY_APOSTROPHE;
            case Key::COMMA:         return GLFW_KEY_COMMA;
            case Key::MINUS:         return GLFW_KEY_MINUS;
            case Key::PERIOD:        return GLFW_KEY_PERIOD;
            case Key::SLASH:         return GLFW_KEY_SLASH;
            case Key::SEMICOLON:     return GLFW_KEY_SEMICOLON;
            case Key::EQUAL:         return GLFW_KEY_EQUAL;
            case Key::LEFT_BRACKET:  return GLFW_KEY_LEFT_BRACKET;
            case Key::BACKSLASH:     return GLFW_KEY_BACKSLASH;
            case Key::RIGHT_BRACKET: return GLFW_KEY_RIGHT_BRACKET;
            case Key::GRAVE_ACCENT:  return GLFW_KEY_GRAVE_ACCENT;
            case Key::WORLD_1:       return GLFW_KEY_WORLD_1;
            case Key::WORLD_2:       return GLFW_KEY_WORLD_2;
            // Navigation / editing
            case Key::LEFT:      return GLFW_KEY_LEFT;
            case Key::RIGHT:     return GLFW_KEY_RIGHT;
            case Key::UP:        return GLFW_KEY_UP;
            case Key::DOWN:      return GLFW_KEY_DOWN;
            case Key::PAGE_UP:   return GLFW_KEY_PAGE_UP;
            case Key::PAGE_DOWN: return GLFW_KEY_PAGE_DOWN;
            case Key::HOME:      return GLFW_KEY_HOME;
            case Key::END:       return GLFW_KEY_END;
            case Key::INSERT:    return GLFW_KEY_INSERT;
            case Key::DEL:       return GLFW_KEY_DELETE;
            case Key::BACKSPACE: return GLFW_KEY_BACKSPACE;
            case Key::TAB:       return GLFW_KEY_TAB;
            // Control
            case Key::SPACE:        return GLFW_KEY_SPACE;
            case Key::ESCAPE:       return GLFW_KEY_ESCAPE;
            case Key::ENTER:        return GLFW_KEY_ENTER;
            case Key::CAPS_LOCK:    return GLFW_KEY_CAPS_LOCK;
            case Key::SCROLL_LOCK:  return GLFW_KEY_SCROLL_LOCK;
            case Key::NUM_LOCK:     return GLFW_KEY_NUM_LOCK;
            case Key::PRINT_SCREEN: return GLFW_KEY_PRINT_SCREEN;
            case Key::PAUSE:        return GLFW_KEY_PAUSE;
            // Function keys
            case Key::F1:  return GLFW_KEY_F1;
            case Key::F2:  return GLFW_KEY_F2;
            case Key::F3:  return GLFW_KEY_F3;
            case Key::F4:  return GLFW_KEY_F4;
            case Key::F5:  return GLFW_KEY_F5;
            case Key::F6:  return GLFW_KEY_F6;
            case Key::F7:  return GLFW_KEY_F7;
            case Key::F8:  return GLFW_KEY_F8;
            case Key::F9:  return GLFW_KEY_F9;
            case Key::F10: return GLFW_KEY_F10;
            case Key::F11: return GLFW_KEY_F11;
            case Key::F12: return GLFW_KEY_F12;
            case Key::F13: return GLFW_KEY_F13;
            case Key::F14: return GLFW_KEY_F14;
            case Key::F15: return GLFW_KEY_F15;
            case Key::F16: return GLFW_KEY_F16;
            case Key::F17: return GLFW_KEY_F17;
            case Key::F18: return GLFW_KEY_F18;
            case Key::F19: return GLFW_KEY_F19;
            case Key::F20: return GLFW_KEY_F20;
            case Key::F21: return GLFW_KEY_F21;
            case Key::F22: return GLFW_KEY_F22;
            case Key::F23: return GLFW_KEY_F23;
            case Key::F24: return GLFW_KEY_F24;
            case Key::F25: return GLFW_KEY_F25;
            // Keypad
            case Key::KP_0:        return GLFW_KEY_KP_0;
            case Key::KP_1:        return GLFW_KEY_KP_1;
            case Key::KP_2:        return GLFW_KEY_KP_2;
            case Key::KP_3:        return GLFW_KEY_KP_3;
            case Key::KP_4:        return GLFW_KEY_KP_4;
            case Key::KP_5:        return GLFW_KEY_KP_5;
            case Key::KP_6:        return GLFW_KEY_KP_6;
            case Key::KP_7:        return GLFW_KEY_KP_7;
            case Key::KP_8:        return GLFW_KEY_KP_8;
            case Key::KP_9:        return GLFW_KEY_KP_9;
            case Key::KP_DECIMAL:  return GLFW_KEY_KP_DECIMAL;
            case Key::KP_DIVIDE:   return GLFW_KEY_KP_DIVIDE;
            case Key::KP_MULTIPLY: return GLFW_KEY_KP_MULTIPLY;
            case Key::KP_SUBTRACT: return GLFW_KEY_KP_SUBTRACT;
            case Key::KP_ADD:      return GLFW_KEY_KP_ADD;
            case Key::KP_ENTER:    return GLFW_KEY_KP_ENTER;
            case Key::KP_EQUAL:    return GLFW_KEY_KP_EQUAL;
            // Modifiers
            case Key::LEFT_SHIFT:    return GLFW_KEY_LEFT_SHIFT;
            case Key::LEFT_CONTROL:  return GLFW_KEY_LEFT_CONTROL;
            case Key::LEFT_ALT:      return GLFW_KEY_LEFT_ALT;
            case Key::LEFT_SUPER:    return GLFW_KEY_LEFT_SUPER;
            case Key::RIGHT_SHIFT:   return GLFW_KEY_RIGHT_SHIFT;
            case Key::RIGHT_CONTROL: return GLFW_KEY_RIGHT_CONTROL;
            case Key::RIGHT_ALT:     return GLFW_KEY_RIGHT_ALT;
            case Key::RIGHT_SUPER:   return GLFW_KEY_RIGHT_SUPER;
            case Key::MENU:          return GLFW_KEY_MENU;
            default:
                spdlog::error("Undefined Key {}", static_cast<int>(key));
                return GLFW_KEY_UNKNOWN;
            };
            // clang-format on
        }

        Key toKeyCode(GLFWKeyCode glfwKeyCode)
        {
            // clang-format off
            switch (glfwKeyCode)
            {
            // Letters
            case GLFW_KEY_A: return Key::A;
            case GLFW_KEY_B: return Key::B;
            case GLFW_KEY_C: return Key::C;
            case GLFW_KEY_D: return Key::D;
            case GLFW_KEY_E: return Key::E;
            case GLFW_KEY_F: return Key::F;
            case GLFW_KEY_G: return Key::G;
            case GLFW_KEY_H: return Key::H;
            case GLFW_KEY_I: return Key::I;
            case GLFW_KEY_J: return Key::J;
            case GLFW_KEY_K: return Key::K;
            case GLFW_KEY_L: return Key::L;
            case GLFW_KEY_M: return Key::M;
            case GLFW_KEY_N: return Key::N;
            case GLFW_KEY_O: return Key::O;
            case GLFW_KEY_P: return Key::P;
            case GLFW_KEY_Q: return Key::Q;
            case GLFW_KEY_R: return Key::R;
            case GLFW_KEY_S: return Key::S;
            case GLFW_KEY_T: return Key::T;
            case GLFW_KEY_U: return Key::U;
            case GLFW_KEY_V: return Key::V;
            case GLFW_KEY_W: return Key::W;
            case GLFW_KEY_X: return Key::X;
            case GLFW_KEY_Y: return Key::Y;
            case GLFW_KEY_Z: return Key::Z;
            // Numbers
            case GLFW_KEY_0: return Key::_0;
            case GLFW_KEY_1: return Key::_1;
            case GLFW_KEY_2: return Key::_2;
            case GLFW_KEY_3: return Key::_3;
            case GLFW_KEY_4: return Key::_4;
            case GLFW_KEY_5: return Key::_5;
            case GLFW_KEY_6: return Key::_6;
            case GLFW_KEY_7: return Key::_7;
            case GLFW_KEY_8: return Key::_8;
            case GLFW_KEY_9: return Key::_9;
            // Punctuation
            case GLFW_KEY_APOSTROPHE:    return Key::APOSTROPHE;
            case GLFW_KEY_COMMA:         return Key::COMMA;
            case GLFW_KEY_MINUS:         return Key::MINUS;
            case GLFW_KEY_PERIOD:        return Key::PERIOD;
            case GLFW_KEY_SLASH:         return Key::SLASH;
            case GLFW_KEY_SEMICOLON:     return Key::SEMICOLON;
            case GLFW_KEY_EQUAL:         return Key::EQUAL;
            case GLFW_KEY_LEFT_BRACKET:  return Key::LEFT_BRACKET;
            case GLFW_KEY_BACKSLASH:     return Key::BACKSLASH;
            case GLFW_KEY_RIGHT_BRACKET: return Key::RIGHT_BRACKET;
            case GLFW_KEY_GRAVE_ACCENT:  return Key::GRAVE_ACCENT;
            case GLFW_KEY_WORLD_1:       return Key::WORLD_1;
            case GLFW_KEY_WORLD_2:       return Key::WORLD_2;
            // Navigation / editing
            case GLFW_KEY_LEFT:      return Key::LEFT;
            case GLFW_KEY_RIGHT:     return Key::RIGHT;
            case GLFW_KEY_UP:        return Key::UP;
            case GLFW_KEY_DOWN:      return Key::DOWN;
            case GLFW_KEY_PAGE_UP:   return Key::PAGE_UP;
            case GLFW_KEY_PAGE_DOWN: return Key::PAGE_DOWN;
            case GLFW_KEY_HOME:      return Key::HOME;
            case GLFW_KEY_END:       return Key::END;
            case GLFW_KEY_INSERT:    return Key::INSERT;
            case GLFW_KEY_DELETE:    return Key::DEL;
            case GLFW_KEY_BACKSPACE: return Key::BACKSPACE;
            case GLFW_KEY_TAB:       return Key::TAB;
            // Control
            case GLFW_KEY_SPACE:        return Key::SPACE;
            case GLFW_KEY_ESCAPE:       return Key::ESCAPE;
            case GLFW_KEY_ENTER:        return Key::ENTER;
            case GLFW_KEY_CAPS_LOCK:    return Key::CAPS_LOCK;
            case GLFW_KEY_SCROLL_LOCK:  return Key::SCROLL_LOCK;
            case GLFW_KEY_NUM_LOCK:     return Key::NUM_LOCK;
            case GLFW_KEY_PRINT_SCREEN: return Key::PRINT_SCREEN;
            case GLFW_KEY_PAUSE:        return Key::PAUSE;
            // Function keys
            case GLFW_KEY_F1:  return Key::F1;
            case GLFW_KEY_F2:  return Key::F2;
            case GLFW_KEY_F3:  return Key::F3;
            case GLFW_KEY_F4:  return Key::F4;
            case GLFW_KEY_F5:  return Key::F5;
            case GLFW_KEY_F6:  return Key::F6;
            case GLFW_KEY_F7:  return Key::F7;
            case GLFW_KEY_F8:  return Key::F8;
            case GLFW_KEY_F9:  return Key::F9;
            case GLFW_KEY_F10: return Key::F10;
            case GLFW_KEY_F11: return Key::F11;
            case GLFW_KEY_F12: return Key::F12;
            case GLFW_KEY_F13: return Key::F13;
            case GLFW_KEY_F14: return Key::F14;
            case GLFW_KEY_F15: return Key::F15;
            case GLFW_KEY_F16: return Key::F16;
            case GLFW_KEY_F17: return Key::F17;
            case GLFW_KEY_F18: return Key::F18;
            case GLFW_KEY_F19: return Key::F19;
            case GLFW_KEY_F20: return Key::F20;
            case GLFW_KEY_F21: return Key::F21;
            case GLFW_KEY_F22: return Key::F22;
            case GLFW_KEY_F23: return Key::F23;
            case GLFW_KEY_F24: return Key::F24;
            case GLFW_KEY_F25: return Key::F25;
            // Keypad
            case GLFW_KEY_KP_0:        return Key::KP_0;
            case GLFW_KEY_KP_1:        return Key::KP_1;
            case GLFW_KEY_KP_2:        return Key::KP_2;
            case GLFW_KEY_KP_3:        return Key::KP_3;
            case GLFW_KEY_KP_4:        return Key::KP_4;
            case GLFW_KEY_KP_5:        return Key::KP_5;
            case GLFW_KEY_KP_6:        return Key::KP_6;
            case GLFW_KEY_KP_7:        return Key::KP_7;
            case GLFW_KEY_KP_8:        return Key::KP_8;
            case GLFW_KEY_KP_9:        return Key::KP_9;
            case GLFW_KEY_KP_DECIMAL:  return Key::KP_DECIMAL;
            case GLFW_KEY_KP_DIVIDE:   return Key::KP_DIVIDE;
            case GLFW_KEY_KP_MULTIPLY: return Key::KP_MULTIPLY;
            case GLFW_KEY_KP_SUBTRACT: return Key::KP_SUBTRACT;
            case GLFW_KEY_KP_ADD:      return Key::KP_ADD;
            case GLFW_KEY_KP_ENTER:    return Key::KP_ENTER;
            case GLFW_KEY_KP_EQUAL:    return Key::KP_EQUAL;
            // Modifiers
            case GLFW_KEY_LEFT_SHIFT:    return Key::LEFT_SHIFT;
            case GLFW_KEY_LEFT_CONTROL:  return Key::LEFT_CONTROL;
            case GLFW_KEY_LEFT_ALT:      return Key::LEFT_ALT;
            case GLFW_KEY_LEFT_SUPER:    return Key::LEFT_SUPER;
            case GLFW_KEY_RIGHT_SHIFT:   return Key::RIGHT_SHIFT;
            case GLFW_KEY_RIGHT_CONTROL: return Key::RIGHT_CONTROL;
            case GLFW_KEY_RIGHT_ALT:     return Key::RIGHT_ALT;
            case GLFW_KEY_RIGHT_SUPER:   return Key::RIGHT_SUPER;
            case GLFW_KEY_MENU:          return Key::MENU;
            default:
                spdlog::error("Undefined Key {}", static_cast<int>(glfwKeyCode));
                return Key::SIZEOF;
            };
            // clang-format on
        }

    }

}
