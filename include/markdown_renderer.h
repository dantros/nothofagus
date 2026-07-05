#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include "imgui_font_id.h"
#include "imgui_image_id.h"

namespace Nothofagus
{

class Canvas;

/// A resolved inline markdown image: which pre-registered ImGui image to draw, plus
/// **optional** per-image width bounds expressed as fractions of the **column width** (the
/// markdown text-wrap width — not the width remaining on the current line, so an inline
/// image near a line wrap is sized like any other).
///
/// With **no bounds** (`MarkdownImage{id}`) the image is drawn at exactly its registered
/// `ImguiImageSize` — uncapped, so it may be small or overflow the column (an overflowing image
/// is clipped at the window edge unless the window has `ImGuiWindowFlags_HorizontalScrollbar`).
/// When bounds are present they clamp the drawn width:
///
///  - `maxWidthPercentage` is a **ceiling**: `width = min(intrinsic, maxPct * column)`.
///    Only shrinks oversized images; always a lossless downscale of the registered handle.
///  - `minWidthPercentage` is a **floor**: `width = max(intrinsic, minPct * column)`.
///    Only enlarges undersized images. Enlarging past the registered rasterization size
///    upscales the fixed-resolution handle — crisp for `Nearest` pixel art, soft for
///    `Linear`; register the source larger if a floored-up image must stay crisp.
///
/// Either bound may be left unset independently (floor-only / ceiling-only). Height follows the
/// image's aspect ratio. Each present bound must lie in `[0, 1]` and `min <= max`; a `debugCheck`
/// fires otherwise (in release, `min > max` lets the floor win and `max > 1` lets it overflow).
struct MarkdownImage
{
    ImguiImageId id;
    std::optional<float> minWidthPercentage;   ///< floor (unset = no floor, draw at registered width).
    std::optional<float> maxWidthPercentage;   ///< ceiling (unset = no cap, may overflow the column).
};

/// Maps a markdown image `src` string (`![alt](src)`) to a pre-registered ImGui image
/// (with optional per-image width bounds). Return `std::nullopt` to skip (a dimmed `[src]`
/// placeholder is drawn). The app registers its document images up front via
/// `Canvas::registerImguiImage` and returns the resulting id here — so declared images have
/// no warm-up. See `MarkdownRenderer::setImageResolver`.
using MarkdownImageResolver = std::function<std::optional<MarkdownImage>(std::string_view src)>;

/**
 * @brief Per-element font mapping for `MarkdownRenderer`.
 *
 * Each slot is optional — when left empty (`std::nullopt`) the renderer falls
 * back to the ImGui font that is current when `print(...)` is called. This
 * lets users opt into bold/italic/heading variants incrementally instead of
 * having to bake the full set up front.
 *
 * Bake fonts via `Canvas::bakeImguiFont(sourceId, sizePx)` and assign the
 * resulting handles here. The same `ImguiFontId` can be reused across slots.
 */
struct MarkdownStyle
{
    std::optional<ImguiFontId> regular;
    std::optional<ImguiFontId> bold;
    std::optional<ImguiFontId> italic;
    std::optional<ImguiFontId> boldItalic;
    std::optional<ImguiFontId> code;                       ///< Monospace, used for inline code and fenced blocks.
    std::array<std::optional<ImguiFontId>, 6> headings{};  ///< h1..h6 — slot index = heading level - 1.

    bool tableBorder = true;
    bool tableHeaderHighlight = true;
};

/**
 * @brief Renders CommonMark / GFM-flavored markdown into the current ImGui window.
 *
 * Wraps the `mekhontsev/imgui_md` library (parser: `mity/md4c`) behind a
 * Nothofagus-style API that uses `ImguiFontId` for font selection — keeping
 * `ImFont*` out of user code. Call `print(text)` inside any ImGui scope
 * (the `Canvas::run()` callback, a `renderImguiTo` callback, or just
 * between an `ImGui::Begin/End` pair).
 *
 * Supported markdown subset (v1): headings (h1..h6), emphasis (bold,
 * italic, bold-italic), inline code, fenced code blocks, unordered and
 * ordered lists with nesting, blockquotes, horizontal rules, tables,
 * links (with optional click callback), and strikethrough.
 *
 * **Inline images (`![alt](src)`)** render an engine sprite when an image
 * resolver is installed via `setImageResolver(...)`: the resolver maps the
 * `src` string to a `MarkdownImage` (a pre-registered `ImguiImageId` plus optional
 * per-image width bounds; see `Canvas::registerImguiImage`), drawn through `imguiImage`
 * and clamped between the bounds (default: fit to the available content width). Without a
 * resolver (or when it returns `std::nullopt`) a dimmed `[src]` placeholder is drawn.
 * Animated / tile-map sources work too — keep the registration live via
 * `Canvas::updateImguiImage`.
 *
 * Construction binds the renderer to a `Canvas&` for font resolution; the
 * canvas must outlive the renderer.
 */
class MarkdownRenderer
{
public:
    /// @param canvas Owning canvas — used to resolve `ImguiFontId` handles to
    ///               `ImFont*` at render time. Must outlive this object.
    explicit MarkdownRenderer(Canvas& canvas);
    ~MarkdownRenderer();

    MarkdownRenderer(const MarkdownRenderer&);
    MarkdownRenderer& operator=(const MarkdownRenderer&);
    MarkdownRenderer(MarkdownRenderer&&) noexcept;
    MarkdownRenderer& operator=(MarkdownRenderer&&) noexcept;

    /// Replace the font / table styling. Safe to call at any point; takes
    /// effect on the next `print()` invocation.
    void setStyle(const MarkdownStyle& style);
    const MarkdownStyle& style() const noexcept;

    /// Set a callback fired when the user clicks a `[text](url)` link (and when an
    /// inline image is clicked, with the image `src`). Receives the raw URL string.
    /// Pass an empty `std::function` to disable.
    void setOpenUrlCallback(std::function<void(std::string_view url)> callback);

    /// Set the resolver that maps an image `src` (`![alt](src)`) to a `MarkdownImage`
    /// (a pre-registered `ImguiImageId` plus optional per-image width bounds; see
    /// `Canvas::registerImguiImage`). The image is drawn inline via `imguiImage`, clamped
    /// between the bounds (default: fit to the available content width). Pass an empty
    /// `std::function` (or have the resolver return `std::nullopt`) to skip an image.
    /// Takes effect on the next `print()`.
    void setImageResolver(MarkdownImageResolver resolver);

    /// Parse and render the given markdown source into the current ImGui
    /// window. Must be called inside an active ImGui frame (between
    /// `ImGui::NewFrame` and `ImGui::Render`, typically inside the
    /// `Canvas::run` update callback or a `renderImguiTo` callback).
    void print(std::string_view markdownText);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace Nothofagus
