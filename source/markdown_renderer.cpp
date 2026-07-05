#include "markdown_renderer.h"

#include "canvas.h"
#include "check.h"
#include "imgui_md.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
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

    // Render an inline image `![alt](src)`: resolve src -> a pre-registered ImGui
    // image and draw it ourselves (fit to content width), then return false so
    // imgui_md's own ImGui::Image is suppressed (we don't double-draw). The price of
    // bypassing its draw is that the title tooltip / click-to-open are re-implemented
    // here. m_href holds the image src.
    bool get_image(image_info&) const override
    {
        const std::string_view src(m_href.data(), m_href.size());

        std::optional<MarkdownImage> image =
            (mImageResolver && mCanvas != nullptr) ? mImageResolver(src) : std::nullopt;

        if (not image)
        {
            // No resolver / unresolved src: a dimmed placeholder keeps the missing
            // image visible (the parser suppresses the alt text while in an image, so
            // the src is the best available label).
            if (mCanvas != nullptr && not src.empty())
                ImGui::TextDisabled("[%.*s]", static_cast<int>(src.size()), src.data());
            return false;
        }

        // With no bounds, draw at exactly the registered ImguiImageSize (uncapped — may overflow,
        // which the window clips unless it has a horizontal scrollbar). With bounds present, clamp
        // the drawn width to fractions of the column (text-wrap) width.
        //
        // Each present bound must lie in [0, 1] with min <= max. In release (no assert), min > max
        // lets the floor win the clamp and max > 1 lets the image overflow the column.
        debugCheck((not image->minWidthPercentage ||
                        (*image->minWidthPercentage >= 0.0f && *image->minWidthPercentage <= 1.0f)) &&
                   (not image->maxWidthPercentage ||
                        (*image->maxWidthPercentage >= 0.0f && *image->maxWidthPercentage <= 1.0f)) &&
                   image->minWidthPercentage.value_or(0.0f) <= image->maxWidthPercentage.value_or(1.0f),
                   "MarkdownImage: width bounds must satisfy 0 <= minWidthPercentage <= maxWidthPercentage <= 1");

        std::optional<glm::vec2> drawSize;
        if (image->minWidthPercentage || image->maxWidthPercentage)
        {
            const glm::vec2 natural = mCanvas->imguiImageSize(image->id);   // logical layout px
            // Basis is the full column (text-wrap) width, NOT GetContentRegionAvail().x (the width
            // *remaining on the current line*). Otherwise an inline image near a line wrap — where
            // little width is left — collapses to a sliver. GetContentRegionMax/GetCursorStartPos are
            // window-local, so their difference is the content column width regardless of cursor X.
            const float columnWidth = ImGui::GetContentRegionMax().x - ImGui::GetCursorStartPos().x;
            if (natural.x > 0.0f && natural.y > 0.0f && columnWidth > 0.0f)
            {
                float width = natural.x;
                if (image->maxWidthPercentage) width = std::min(width, *image->maxWidthPercentage * columnWidth); // ceiling
                if (image->minWidthPercentage) width = std::max(width, *image->minWidthPercentage * columnWidth); // floor
                if (width != natural.x)
                    drawSize = glm::vec2(width, width * natural.y / natural.x);             // height follows aspect
            }
        }

        mCanvas->imguiImage(image->id, drawSize);   // nullopt -> registered ImguiImageSize (raw)

        // imgui_md's own hover/click handling is bypassed (we returned false), so
        // re-create it on the image item: tooltip with the src, click fires open_url.
        if (ImGui::IsItemHovered())
        {
            if (not src.empty())
                ImGui::SetTooltip("%.*s", static_cast<int>(src.size()), src.data());
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                open_url();
        }
        return false;
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
