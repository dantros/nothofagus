#pragma once

#include <glm/glm.hpp>
#include <variant>

namespace Nothofagus
{

/// How a Visual's content maps into a `Custom` / `DevicePixels` target box when the
/// content's aspect ratio differs from the box.
enum class ImguiImageFit
{
    Stretch, ///< Map the content's bounding box onto the full target (may distort).
    Fit,     ///< Uniform scale to fit inside the target, centered (letterbox/pillarbox).
};

/// Sizing alternatives for `Canvas::imguiVisual`. `LogicalPixels` / `Scaled` / `Custom`
/// size in **logical pixels** (they scale with the OS DPI, like the rest of the UI) and
/// rasterize the internal render target at size × content scale, so mesh geometry stays
/// crisp at the displayed size instead of being bitmap-upscaled. `DevicePixels` instead
/// sizes in **physical/device pixels**, bypassing OS DPI scaling for a 1:1 mapping.
///
/// Each alternative carries exactly the data it needs — `fit` only exists where a target
/// box can differ in aspect from the content, and a `factor` (multiplier) vs a `size`
/// (absolute extent) are distinct fields rather than one overloaded value.
/// `ImguiImageSize::Spec` is the variant over the four.
namespace ImguiImageSize
{

/// The visual's natural size: its mesh AABB extent in logical px (no scaling). The default.
/// Logical px scale with the OS DPI, so the image grows on HiDPI like the surrounding UI.
struct LogicalPixels
{
};

/// The natural size multiplied per-axis by `factor` before rasterization (up/downscale).
/// Logical px (DPI-scaled).
struct Scaled
{
    glm::vec2 factor{1.0f, 1.0f};
};

/// An explicit target size in logical px (DPI-scaled), with the content placed per `fit`.
/// `Fit` letterboxes/pillarboxes to preserve the content's proportions; `Stretch` fills (distorts).
struct Custom
{
    glm::vec2     size{1.0f, 1.0f};
    ImguiImageFit fit = ImguiImageFit::Fit;
};

/// An explicit target size in **physical/device pixels** — 1 unit = 1 display pixel,
/// bypassing OS DPI scaling. A 100×100 texture shown at `{100, 100}` occupies exactly
/// 100×100 screen pixels on any display (so on HiDPI it appears physically smaller than a
/// logical-pixel image of the same number). `fit` places the content when its aspect
/// differs from the box.
struct DevicePixels
{
    glm::vec2     size{1.0f, 1.0f};
    ImguiImageFit fit = ImguiImageFit::Fit;
};

/// The sizing spec passed to `Canvas::imguiVisual` — a variant over the four alternatives.
/// Default-constructs (via the first alternative) to `LogicalPixels`.
using Spec = std::variant<LogicalPixels, Scaled, Custom, DevicePixels>;

} // namespace ImguiImageSize

} // namespace Nothofagus
