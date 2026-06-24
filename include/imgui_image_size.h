#pragma once

#include <glm/glm.hpp>
#include <variant>

namespace Nothofagus
{

/// How a Visual's content maps into an `Explicit` target box when the content's aspect
/// ratio differs from the box.
enum class ImguiImageFit
{
    Stretch, ///< Map the content's bounding box onto the full target (may distort).
    Fit,     ///< Uniform scale to fit inside the target, centered (letterbox/pillarbox).
};

/// The pixel units a size is expressed in — an orthogonal axis to the size source.
enum class ImguiImageUnits
{
    Logical, ///< Logical pixels: scale with the OS DPI, like the rest of the UI.
    Device,  ///< Physical/device pixels: 1 unit = 1 display pixel, bypassing OS DPI scaling.
};

/// Sizing alternatives for `Canvas::registerImguiImage`, along two orthogonal axes: the **size
/// source** (which variant alternative — `Natural` / `Scaled` / `Explicit`) and the
/// **units** (the `ImguiImageUnits` field on each, default `Logical`).
///
/// In `Logical` units the off-screen target is rasterized at size × content scale (the same
/// DPI density the font atlas uses), so the image is DPI-scaled like the rest of the UI and
/// mesh geometry stays crisp at the displayed size. In `Device` units the size is in physical
/// pixels (1 texel → 1 display pixel), bypassing OS DPI — e.g. `Natural{Device}` shows a
/// texture at its native resolution and `Scaled{4, Device}` is a crisp 4× pixel-art zoom.
/// `ImguiImageSize::Spec` is the variant over the three size sources.
namespace ImguiImageSize
{

/// The visual's natural size (its mesh AABB extent). In `Logical` units it is the real
/// on-screen size (DPI-scaled); in `Device` units it is the texture's native resolution
/// (1 texel → 1 display pixel). The default.
struct Natural
{
    ImguiImageUnits units = ImguiImageUnits::Logical;
};

/// The natural size multiplied per-axis by `factor`. In `Device` units this is a crisp
/// integer pixel-art zoom (exactly factor × native physical pixels).
struct Scaled
{
    glm::vec2       factor{1.0f, 1.0f};
    ImguiImageUnits units = ImguiImageUnits::Logical;
};

/// An explicit target size, with the content placed per `fit`. `Logical` units are
/// DPI-scaled; `Device` units are exact physical pixels. `Fit` letterboxes/pillarboxes to
/// preserve the content's proportions; `Stretch` fills the box (may distort).
struct Explicit
{
    glm::vec2       size{1.0f, 1.0f};
    ImguiImageFit   fit   = ImguiImageFit::Fit;
    ImguiImageUnits units = ImguiImageUnits::Logical;
};

/// The sizing spec passed to `Canvas::registerImguiImage` — a variant over the three size
/// sources. Default-constructs (via the first alternative) to `Natural` in `Logical` units.
using Spec = std::variant<Natural, Scaled, Explicit>;

} // namespace ImguiImageSize

} // namespace Nothofagus
