#include <nothofagus.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <cstdlib>

int main()
{
    Nothofagus::Canvas canvas({320, 240}, "Hello Markdown", {0.10f, 0.10f, 0.14f}, 3);

    // Wire up the markdown typeset from the canvas's embedded Noto Sans family.
    // defaultMarkdownStyle bakes true regular / bold / italic / bold-italic
    // faces plus a monospace face for code and descending heading sizes — so
    // **strong**, *emphasis*, ***both***, and `inline code` all render as
    // distinct faces. Called before run() so the bakes are synchronous.
    Nothofagus::MarkdownRenderer markdown(canvas);
    markdown.setStyle(canvas.defaultMarkdownStyle(16.0f));
    markdown.setOpenUrlCallback([](std::string_view url) {
        spdlog::info("Markdown link clicked: {}", url);
    });

    // Build a small RGBA logo texture procedurally and expose it to markdown as
    // an inline image. The resolver maps an image `src` string to a TextureId;
    // here `![logo](tex:logo)` resolves to this texture. Note the texture is
    // referenced by no bellota — the markdown image bridge takes an independent
    // RGBA snapshot, so the per-frame texture GC reclaiming the source is fine.
    constexpr int kLogoSize = 32;
    Nothofagus::DirectTexture logo(glm::ivec2{kLogoSize, kLogoSize});
    for (int j = 0; j < kLogoSize; ++j)
        for (int i = 0; i < kLogoSize; ++i)
        {
            const float u = static_cast<float>(i) / (kLogoSize - 1);
            const float v = static_cast<float>(j) / (kLogoSize - 1);
            const bool border = (i < 2 || j < 2 || i >= kLogoSize - 2 || j >= kLogoSize - 2);
            const glm::vec4 color = border ? glm::vec4{0.95f, 0.85f, 0.20f, 1.0f}
                                           : glm::vec4{u, 0.55f, v, 1.0f};
            logo.setColor(i, j, color);
        }
    const Nothofagus::TextureId logoTextureId = canvas.addTexture(logo);

    // A paletted (indirect) texture: pixels are palette indices, resolved to
    // RGBA on the CPU when bridged to an inline image. Single layer + no
    // tile-map grid, so it flattens through the same path as the direct logo.
    constexpr int kBadgeSize = 16;
    Nothofagus::IndirectTexture badge(glm::ivec2{kBadgeSize, kBadgeSize}, glm::vec4{0.0f, 0.0f, 0.0f, 0.0f});
    badge.setPallete(Nothofagus::ColorPallete{
        {0.00f, 0.00f, 0.00f, 0.00f},  // 0: transparent
        {0.15f, 0.80f, 0.90f, 1.00f},  // 1: cyan ring
        {0.90f, 0.20f, 0.60f, 1.00f},  // 2: magenta core
    });
    for (int j = 0; j < kBadgeSize; ++j)
        for (int i = 0; i < kBadgeSize; ++i)
        {
            const int distance = std::abs(i - kBadgeSize / 2) + std::abs(j - kBadgeSize / 2);
            const Nothofagus::Pixel::ColorId id = (distance < 4) ? 2 : (distance < 7 ? 1 : 0);
            badge.setPixel(i, j, Nothofagus::Pixel{id});
        }
    const Nothofagus::TextureId badgeTextureId = canvas.addTexture(badge);

    markdown.setImageResolver([logoTextureId, badgeTextureId](std::string_view src)
        -> std::optional<Nothofagus::TextureId>
    {
        if (src == "tex:logo")  return logoTextureId;
        if (src == "tex:badge") return badgeTextureId;
        return std::nullopt;
    });

    static constexpr const char* kSample = R"md(# Hello Markdown

Welcome to **Nothofagus** markdown rendering. This panel shows
*emphasis*, **strong**, ***both***, and `inline code`.

## Inline images

A direct (RGBA) texture and a paletted (indirect) texture, drawn inline:

![logo](tex:logo) ![badge](tex:badge)

## Lists

- Apples
- Oranges
  - Sub-item one
  - Sub-item two
- Bananas

1. First
2. Second
3. Third

## Code block

```cpp
canvas.run([&](float dt) {
    markdown.print("# Heading");
});
```

## Tables

| feature       | supported |
|---------------|-----------|
| headings      | yes       |
| code blocks   | yes       |
| tables        | yes       |
| inline images | yes       |

> Blockquotes work too. See [the repo](https://github.com/dantros/nothofagus)
> for the project source. Strikethrough: ~~deprecated~~.

---

The horizontal rule above closes the document.
)md";

    canvas.run([&](float)
    {
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(640.0f, 600.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Readme");
        markdown.print(kSample);
        ImGui::End();
    });

    return 0;
}
