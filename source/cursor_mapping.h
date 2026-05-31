#pragma once

#include "canvas.h"
#include <glm/vec2.hpp>

namespace Nothofagus
{

/// Compute the letterbox/pillarbox viewport rect that preserves the canvas aspect
/// ratio inside the given framebuffer size. Used both by the per-frame render loop
/// (FrameRunner) and by per-event cursor mapping in the window backends so they
/// agree on the math even when the framebuffer is in flux (resize events arriving
/// mid-poll).
inline ViewportRect computeLetterboxViewport(
    int framebufferWidth, int framebufferHeight,
    unsigned int canvasWidth, unsigned int canvasHeight)
{
    const float canvasAspectRatio      = static_cast<float>(canvasWidth)      / static_cast<float>(canvasHeight);
    const float framebufferAspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    int viewportWidth, viewportHeight, viewportX, viewportY;
    if (framebufferAspectRatio > canvasAspectRatio)
    {   // Pillarbox: framebuffer is wider than canvas — black bands left and right
        viewportHeight = framebufferHeight;
        viewportWidth  = static_cast<int>(framebufferHeight * canvasAspectRatio);
        viewportX      = (framebufferWidth - viewportWidth) / 2;
        viewportY      = 0;
    }
    else
    {   // Letterbox: framebuffer is taller than canvas — black bands top and bottom
        viewportWidth  = framebufferWidth;
        viewportHeight = static_cast<int>(framebufferWidth / canvasAspectRatio);
        viewportX      = 0;
        viewportY      = (framebufferHeight - viewportHeight) / 2;
    }
    return { viewportX, viewportY, viewportWidth, viewportHeight };
}

/// Map a window-space cursor position (top-left origin, in window coords) to game-canvas
/// coordinates (bottom-left origin, in logical canvas pixels).
///
/// Window and framebuffer sizes are passed in fresh by the caller so that resize events
/// arriving mid-poll-cycle do not leave the cursor anchored to stale dimensions. The
/// letterbox viewport is recomputed locally from those fresh dimensions to fix a second-
/// order staleness — the per-frame viewport captured by FrameRunner before the poll loop
/// would otherwise be one frame behind for any cursor event firing after a resize event.
inline glm::vec2 mapWindowCursorToCanvas(
    float cursorXTopLeft, float cursorYTopLeft,
    int windowWidth, int windowHeight,
    int framebufferWidth, int framebufferHeight,
    const ScreenSize& canvasSize)
{
    const float scaleX = (windowWidth  > 0) ? static_cast<float>(framebufferWidth)  / static_cast<float>(windowWidth)  : 1.0f;
    const float scaleY = (windowHeight > 0) ? static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight) : 1.0f;

    // Convert to framebuffer coords with bottom-left origin.
    const float fbCursorX = cursorXTopLeft * scaleX;
    const float fbCursorY = static_cast<float>(framebufferHeight) - cursorYTopLeft * scaleY;

    const ViewportRect viewport = computeLetterboxViewport(
        framebufferWidth, framebufferHeight, canvasSize.width, canvasSize.height);

    const float viewportWidth  = (viewport.width  > 0) ? static_cast<float>(viewport.width)  : 1.0f;
    const float viewportHeight = (viewport.height > 0) ? static_cast<float>(viewport.height) : 1.0f;

    return {
        (fbCursorX - static_cast<float>(viewport.x)) / viewportWidth  * static_cast<float>(canvasSize.width),
        (fbCursorY - static_cast<float>(viewport.y)) / viewportHeight * static_cast<float>(canvasSize.height)
    };
}

} // namespace Nothofagus
