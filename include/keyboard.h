
#pragma once

namespace Nothofagus
{

// clang-format off
enum class Key : unsigned int
{
    // Letters
    A = 0, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Numbers
    _0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
    // Punctuation
    APOSTROPHE, COMMA, MINUS, PERIOD, SLASH,
    SEMICOLON, EQUAL,
    LEFT_BRACKET, BACKSLASH, RIGHT_BRACKET, GRAVE_ACCENT,
    WORLD_1, WORLD_2,
    // Navigation / editing
    LEFT, RIGHT, UP, DOWN,
    PAGE_UP, PAGE_DOWN, HOME, END,
    INSERT, DEL, BACKSPACE, TAB,
    // Control
    SPACE, ESCAPE, ENTER,
    CAPS_LOCK, SCROLL_LOCK, NUM_LOCK, PRINT_SCREEN, PAUSE,
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
    // Keypad
    KP_0, KP_1, KP_2, KP_3, KP_4, KP_5, KP_6, KP_7, KP_8, KP_9,
    KP_DECIMAL, KP_DIVIDE, KP_MULTIPLY, KP_SUBTRACT, KP_ADD, KP_ENTER, KP_EQUAL,
    // Modifiers
    LEFT_SHIFT, LEFT_CONTROL, LEFT_ALT, LEFT_SUPER,
    RIGHT_SHIFT, RIGHT_CONTROL, RIGHT_ALT, RIGHT_SUPER,
    MENU,
    SIZEOF
};
// clang-format on

namespace KeyboardImplementation
{
    using InternalKeyCode = int;

    InternalKeyCode toInternalKeyCode(Key key);

    Key toKeyCode(InternalKeyCode code);
}

}
