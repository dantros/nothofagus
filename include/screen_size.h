#pragma once

namespace Nothofagus
{

/// @struct ScreenSize
/// @brief Structure representing the size of the screen in pixels.
struct ScreenSize
{
    unsigned int width, height;
};

constexpr inline bool operator!=(const ScreenSize& lhs, const ScreenSize& rhs)
{
    return lhs.width != rhs.width or lhs.height != rhs.height;
}

constexpr inline bool operator==(const ScreenSize& lhs, const ScreenSize& rhs)
{
    return lhs.width == rhs.width and lhs.height == rhs.height;
}

/// @struct ViewportRect
/// @brief Game viewport rectangle in framebuffer pixels (OpenGL convention: y from bottom).
struct ViewportRect { int x, y, width, height; };

} // namespace Nothofagus
