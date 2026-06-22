#pragma once

#include <glm/glm.hpp>
#include <variant>

namespace Nothofagus
{

/// How a Visual's content maps into an `ImguiImageCustom` target size when the
/// content's aspect ratio differs from the target box.
enum class ImguiImageFit
{
    Stretch, ///< Map the content's bounding box onto the full target (may distort).
    Fit,     ///< Uniform scale to fit inside the target, centered (letterbox/pillarbox).
};

/// Sizing alternatives for `Canvas::imguiVisual`. All sizes are in **logical pixels**
/// (they scale with the OS DPI, like the rest of the UI). The internal render target is
/// rasterized at the resulting size × content scale, so mesh geometry stays crisp at the
/// displayed size instead of being bitmap-upscaled.
///
/// Each alternative carries exactly the data it needs — `fit` only exists on the `Custom`
/// alternative, and `factor` (a multiplier) vs `size` (an absolute extent) are distinct
/// fields rather than one overloaded value.

/// The visual's natural size: its mesh AABB extent in logical px (no scaling).
struct ImguiImageStandard
{
};

/// The natural size multiplied per-axis by `factor`.
struct ImguiImageScaled
{
    glm::vec2 factor{1.0f, 1.0f};
};

/// An explicit target size (logical px), with the content placed per `fit`.
struct ImguiImageCustom
{
    glm::vec2     size{1.0f, 1.0f};
    ImguiImageFit fit = ImguiImageFit::Fit;
};

/// Sizing spec for `Canvas::imguiVisual` — a variant over the three alternatives above.
/// Named factories keep construction terse; default-constructs to `ImguiImageStandard`.
struct ImguiImageSize : std::variant<ImguiImageStandard, ImguiImageScaled, ImguiImageCustom>
{
    using variant::variant;

    /// The visual's real on-screen size (mesh AABB extent), no scaling. The default.
    static ImguiImageSize standard() { return ImguiImageStandard{}; }

    /// Multiply the natural size uniformly by `s` before rasterization (up/downscale).
    static ImguiImageSize scaled(float s) { return ImguiImageScaled{{s, s}}; }

    /// Multiply the natural size per-axis by `s` before rasterization.
    static ImguiImageSize scaled(glm::vec2 s) { return ImguiImageScaled{s}; }

    /// Render at an explicit target size (logical px). `Fit` letterboxes/pillarboxes
    /// to preserve the content's proportions; `Stretch` fills the box (may distort).
    static ImguiImageSize custom(glm::vec2 sizeLogical, ImguiImageFit fit = ImguiImageFit::Fit)
    {
        return ImguiImageCustom{sizeLogical, fit};
    }
};

} // namespace Nothofagus
