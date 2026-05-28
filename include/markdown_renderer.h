#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include "imgui_font_id.h"

namespace Nothofagus
{

class Canvas;

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
 * **Inline images (`![alt](url)`) are not rendered in v1.** The parser still
 * consumes them and skips them silently. See the project roadmap for
 * image support — it requires a separate `TextureId → ImTextureID` bridge
 * that handles the engine's layered (2D-array) texture format.
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

    MarkdownRenderer(const MarkdownRenderer&) = delete;
    MarkdownRenderer& operator=(const MarkdownRenderer&) = delete;
    MarkdownRenderer(MarkdownRenderer&&) noexcept;
    MarkdownRenderer& operator=(MarkdownRenderer&&) noexcept;

    /// Replace the font / table styling. Safe to call at any point; takes
    /// effect on the next `print()` invocation.
    void setStyle(const MarkdownStyle& style);
    const MarkdownStyle& style() const noexcept;

    /// Set a callback fired when the user clicks a `[text](url)` link.
    /// Receives the raw URL string. Pass an empty `std::function` to disable.
    void setOpenUrlCallback(std::function<void(std::string_view url)> callback);

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
