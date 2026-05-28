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

    // v1 deliberately does not render inline images. The base class's default
    // would draw the ImGui font atlas as a stand-in, which is worse than just
    // skipping. Returning false leaves the image out entirely.
    bool get_image(image_info&) const override
    {
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

void MarkdownRenderer::print(std::string_view markdownText)
{
    const char* begin = markdownText.data();
    const char* end   = begin + markdownText.size();
    mImpl->print(begin, end);
}

} // namespace Nothofagus
