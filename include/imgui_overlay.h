#pragma once

namespace Nothofagus
{

/// An overlay rectangle expressed in ImGui display coordinates: top-left origin,
/// y growing downward — the space expected by ImGui::SetNextWindowPos/Size.
struct ImguiOverlayRect { float x, y, width, height; };

/// Convert a game viewport (framebuffer pixels, bottom-left origin — as returned
/// by Canvas::gameViewport()) into an ImGui display-space rect (top-left origin,
/// in ImGui "points"), accounting for DisplayFramebufferScale.
///
/// ImGui places windows in DisplaySize ("point") units; the renderer multiplies
/// by DisplayFramebufferScale to reach framebuffer pixels. On a scaled (HiDPI)
/// display the two spaces differ by that factor, so feeding raw framebuffer-pixel
/// viewport values straight into SetNextWindowPos/Size mis-places overlays. This
/// helper is the single conversion point shared by the engine bindings and the
/// overlay tests so the math lives in exactly one place.
///
/// Inputs:
///   gameViewport*          — Canvas::gameViewport(), framebuffer pixels, y-up.
///   displayWidth/Height    — ImGui::GetIO().DisplaySize, points.
///   displayFramebufferScale* — ImGui::GetIO().DisplayFramebufferScale
///                              (framebuffer pixels per point; framebuffer height
///                              in pixels = displayHeight * scaleY).
///
/// Pure and header-only so the nonvisual (no-render-backend) test group can
/// include it without pulling in imgui.h or a render backend.
inline ImguiOverlayRect computeImguiOverlayViewport(
    int gameViewportX, int gameViewportY, int gameViewportWidth, int gameViewportHeight,
    float displayWidth, float displayHeight,
    float displayFramebufferScaleX, float displayFramebufferScaleY)
{
    const float scaleX = (displayFramebufferScaleX > 0.0f) ? displayFramebufferScaleX : 1.0f;
    const float scaleY = (displayFramebufferScaleY > 0.0f) ? displayFramebufferScaleY : 1.0f;

    const float x      = static_cast<float>(gameViewportX)      / scaleX;
    const float width  = static_cast<float>(gameViewportWidth)  / scaleX;
    const float height = static_cast<float>(gameViewportHeight) / scaleY;

    // gameViewport().y is measured from the bottom of the framebuffer; ImGui's y
    // grows downward from the top of the (point-space) display. Convert the top
    // edge: displayHeight - (viewport top, in points). Tracks the top of the game
    // content even when letterboxed (viewport.y > 0).
    const float y = displayHeight - static_cast<float>(gameViewportY + gameViewportHeight) / scaleY;

    return { x, y, width, height };
}

} // namespace Nothofagus
