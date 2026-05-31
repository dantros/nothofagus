#include "markdown_renderer.h"

#include "canvas.h"
#include "check.h"
#include "imgui_md.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace Nothofagus
{

struct MarkdownRenderer::Impl : public imgui_md
{
    explicit Impl(Canvas& canvas) noexcept
        : mCanvas(&canvas)
    {
    }

    void setStyle(const MarkdownStyle& style)
    {
        mStyle = style;
        m_table_border           = style.tableBorder;
        m_table_header_highlight = style.tableHeaderHighlight;
    }

    const MarkdownStyle& style() const noexcept { return mStyle; }

    void setOpenUrlCallback(std::function<void(std::string_view)> callback)
    {
        mOpenUrlCallback = std::move(callback);
    }

    void setImageResolver(MarkdownImageResolver resolver)
    {
        mImageResolver = std::move(resolver);
    }

    // The regular body font, resolved (or nullptr if unset / not yet baked).
    // print() pushes this so PLAIN paragraph text uses the markdown family
    // rather than whatever ImGui font happens to be current (the main HiDPI
    // font) — otherwise styled spans and plain text render at different sizes.
    ImFont* bodyFont() const
    {
        return mStyle.regular ? resolve(*mStyle.regular) : nullptr;
    }

protected:
    ImFont* get_font() const override
    {
        // Heading levels override everything else; spec follows imgui_md's
        // convention of m_hlevel == 0 meaning body text.
        if (m_hlevel >= 1 && m_hlevel <= 6)
        {
            if (auto headingId = mStyle.headings[m_hlevel - 1])
                return resolve(*headingId);
        }

        if (m_is_code)
        {
            if (auto codeId = mStyle.code)
                return resolve(*codeId);
        }

        if (m_is_strong && m_is_em)
        {
            if (auto id = mStyle.boldItalic) return resolve(*id);
            // Fall back to bold or italic individually if bold-italic is missing.
            if (auto id = mStyle.bold)       return resolve(*id);
            if (auto id = mStyle.italic)     return resolve(*id);
        }
        else if (m_is_strong)
        {
            if (auto id = mStyle.bold)       return resolve(*id);
        }
        else if (m_is_em)
        {
            if (auto id = mStyle.italic)     return resolve(*id);
        }

        if (auto id = mStyle.regular)        return resolve(*id);

        return nullptr;  // imgui_md interprets nullptr as "use current font"
    }

    // A markdown soft line break (a single '\n' inside a paragraph) renders as
    // a space in CommonMark. imgui_md's base soft_break() is a no-op, which
    // jams the two words together ("shows*emphasis*"). Emit a single space in
    // the current font and stay on the line.
    void soft_break() override
    {
        ImGui::TextUnformatted(" ");
        ImGui::SameLine(0.0f, 0.0f);
    }

    // imgui_md's SPAN_CODE / BLOCK_CODE only toggle state — they never push a
    // font — so inline code and fenced blocks would render in the proportional
    // body font. Push the monospace face ourselves (mirrors the base class's
    // private set_font()) so `code` and ``` blocks are actually monospaced.
    // get_font() reads m_is_code, so set the flag before resolving the font.
    void SPAN_CODE(bool enter) override
    {
        if (enter) { m_is_code = true;  ImGui::PushFont(get_font()); }
        else       { ImGui::PopFont();  m_is_code = false; }
    }

    void BLOCK_CODE(const MD_BLOCK_CODE_DETAIL*, bool enter) override
    {
        if (enter) { m_is_code = true;  ImGui::PushFont(get_font()); }
        else       { ImGui::PopFont();  m_is_code = false; }
    }

    // Resolve a markdown image `src` to an engine texture and hand ImGui a 2D
    // handle for it. Returns false (image skipped) when no resolver is set, the
    // resolver declines the src, or the canvas can't bridge the texture.
    bool get_image(image_info& nfo) const override
    {
        if (!mImageResolver || mCanvas == nullptr)
            return false;

        std::optional<TextureId> textureIdOpt =
            mImageResolver(std::string_view(m_href.data(), m_href.size()));
        if (!textureIdOpt)
            return false;

        const std::uint64_t handle = mCanvas->imguiImageHandle(*textureIdOpt);
        if (handle == 0)
            return false;

        // Read the size from the bridge cache rather than the live texture — a
        // markdown-only image is referenced by no bellota, so the source may be
        // released by the per-frame texture GC after the first frame.
        const glm::ivec2 size = mCanvas->imguiImageSize(*textureIdOpt);
        nfo.texture_id = static_cast<ImTextureID>(handle);
        nfo.size       = ImVec2(static_cast<float>(size.x), static_cast<float>(size.y));
        nfo.uv0        = ImVec2(0.0f, 0.0f);
        nfo.uv1        = ImVec2(1.0f, 1.0f);
        nfo.col_tint   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        nfo.col_border = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        return true;
    }

    void open_url() const override
    {
        if (mOpenUrlCallback)
            mOpenUrlCallback(std::string_view(m_href.data(), m_href.size()));
    }

private:
    ImFont* resolve(ImguiFontId id) const
    {
        debugCheck(mCanvas != nullptr, "MarkdownRenderer has unbound canvas");
        if (mCanvas == nullptr)
        {
            spdlog::error("MarkdownRenderer::resolve called on a renderer with an unbound canvas");
            return nullptr;  // unbound canvas — fall back to current font
        }
        if (!mCanvas->isImguiFontReady(id))
            return nullptr;  // deferred-bake window — defer to current font
        return mCanvas->getImguiFontPtr(id);
    }

    Canvas* mCanvas;
    MarkdownStyle mStyle{};
    std::function<void(std::string_view)> mOpenUrlCallback;
    MarkdownImageResolver mImageResolver;
};

MarkdownRenderer::MarkdownRenderer(Canvas& canvas)
    : mImpl(std::make_unique<Impl>(canvas))
{
}

MarkdownRenderer::~MarkdownRenderer() = default;
MarkdownRenderer::MarkdownRenderer(MarkdownRenderer&&) noexcept = default;
MarkdownRenderer& MarkdownRenderer::operator=(MarkdownRenderer&&) noexcept = default;

MarkdownRenderer::MarkdownRenderer(const MarkdownRenderer& other)
    : mImpl(std::make_unique<Impl>(*other.mImpl))
{
}

MarkdownRenderer& MarkdownRenderer::operator=(const MarkdownRenderer& other)
{
    if (this != &other)
        mImpl = std::make_unique<Impl>(*other.mImpl);
    return *this;
}

void MarkdownRenderer::setStyle(const MarkdownStyle& style)
{
    mImpl->setStyle(style);
}

const MarkdownStyle& MarkdownRenderer::style() const noexcept
{
    return mImpl->style();
}

void MarkdownRenderer::setOpenUrlCallback(std::function<void(std::string_view)> callback)
{
    mImpl->setOpenUrlCallback(std::move(callback));
}

void MarkdownRenderer::setImageResolver(MarkdownImageResolver resolver)
{
    mImpl->setImageResolver(std::move(resolver));
}

void MarkdownRenderer::print(std::string_view markdownText)
{
    const char* begin = markdownText.data();
    const char* end   = begin + markdownText.size();

    // Push the regular body font so plain paragraph text matches the styled
    // spans (imgui_md only pushes fonts for headings/bold/italic/code; without
    // this, plain text falls back to the ambient main-canvas font).
    ImFont* body = mImpl->bodyFont();
    if (body) ImGui::PushFont(body);
    mImpl->print(begin, end);
    if (body) ImGui::PopFont();
}

} // namespace Nothofagus
