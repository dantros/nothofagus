#pragma once

#include <glm/glm.hpp>
#include <variant>

namespace Nothofagus
{

/// How a Visual's content maps into an `ImguiImageSize::Custom` target size when the
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
/// Each alternative carries exactly the data it needs — `fit` only exists on `Custom`,
/// and `factor` (a multiplier) vs `size` (an absolute extent) are distinct fields rather
/// than one overloaded value. `ImguiImageSize::Spec` is the variant over the three.
namespace ImguiImageSize
{

/// The visual's natural size: its mesh AABB extent in logical px (no scaling). The default.
struct Standard
{
};

/// The natural size multiplied per-axis by `factor` before rasterization (up/downscale).
struct Scaled
{
    glm::vec2 factor{1.0f, 1.0f};
};

/// An explicit target size (logical px), with the content placed per `fit`. `Fit`
/// letterboxes/pillarboxes to preserve the content's proportions; `Stretch` fills (distorts).
struct Custom
{
    glm::vec2     size{1.0f, 1.0f};
    ImguiImageFit fit = ImguiImageFit::Fit;
};

/// The sizing spec passed to `Canvas::imguiVisual` — a variant over the three alternatives.
/// Default-constructs (via the first alternative) to `Standard`.
using Spec = std::variant<Standard, Scaled, Custom>;

} // namespace ImguiImageSize

} // namespace Nothofagus
