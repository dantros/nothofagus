#pragma once

#include <cstdint>

namespace Nothofagus
{

/// @brief Swapchain / vsync presentation preference, selected at Canvas construction.
///
/// Maps onto a Vulkan VkPresentModeKHR (windowed) and an OpenGL swap interval.
/// On Vulkan, an unsupported mode silently falls back to Fifo (vk-bootstrap).
/// OpenGL has no Mailbox, so Mailbox and Fifo both map to swap interval 1.
enum class PresentMode : std::uint8_t
{
    Fifo      = 0,  ///< Mandatory vsync, double-buffered. On a compositor this can quantize to ~45 fps.
    Mailbox   = 1,  ///< Vsync'd, triple-buffered, no tearing — compositor-friendly. The default.
    Immediate = 2,  ///< Uncapped, may tear. Useful for benchmarking.
};

/// OpenGL / SDL GL swap interval for a present mode. GL has no Mailbox, so
/// Fifo and Mailbox both request vsync (interval 1); Immediate disables it (0).
constexpr int presentModeToSwapInterval(PresentMode mode)
{
    return mode == PresentMode::Immediate ? 0 : 1;
}

} // namespace Nothofagus
