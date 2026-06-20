#pragma once

#include <glm/glm.hpp>

namespace Nothofagus
{

/// How a Visual's content maps into a `custom` target size when the content's
/// aspect ratio differs from the target box.
enum class ImguiImageFit
{
    Stretch, ///< Map the content's bounding box onto the full target (may distort).
    Fit,     ///< Uniform scale to fit inside the target, centered (letterbox/pillarbox).
};

/// Sizing spec for `Canvas::imguiVisual`. All sizes are in **logical pixels** (they
/// scale with the OS DPI, like the rest of the UI). The internal render target is
/// rasterized at the resulting size × content scale, so mesh geometry stays crisp
/// at the displayed size instead of being bitmap-upscaled.
struct ImguiImageSize
{
    enum class Mode
    {
        Standard, ///< The visual's natural size (its mesh AABB extent in logical px).
        Scaled,   ///< Natural size multiplied per-axis by `value`.
        Custom,   ///< An explicit target size `value` (logical px), placed per `fit`.
    };

    Mode          mode  = Mode::Standard;
    glm::vec2     value = {1.0f, 1.0f};       ///< Scaled: per-axis factor. Custom: target size (logical px).
    ImguiImageFit fit   = ImguiImageFit::Fit; ///< Custom only.

    /// The visual's real on-screen size (mesh AABB extent), no scaling. The default.
    static ImguiImageSize standard() { return {Mode::Standard, {1.0f, 1.0f}, ImguiImageFit::Fit}; }

    /// Multiply the natural size uniformly by `s` before rasterization (up/downscale).
    static ImguiImageSize scaled(float s) { return {Mode::Scaled, {s, s}, ImguiImageFit::Fit}; }

    /// Multiply the natural size per-axis by `s` before rasterization.
    static ImguiImageSize scaled(glm::vec2 s) { return {Mode::Scaled, s, ImguiImageFit::Fit}; }

    /// Render at an explicit target size (logical px). `Fit` letterboxes/pillarboxes
    /// to preserve the content's proportions; `Stretch` fills the box (may distort).
    static ImguiImageSize custom(glm::vec2 sizeLogical, ImguiImageFit fit = ImguiImageFit::Fit)
    {
        return {Mode::Custom, sizeLogical, fit};
    }
};

} // namespace Nothofagus
