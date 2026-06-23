#include <nothofagus.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <vector>

namespace
{
    // A 16x16 paletted diamond "logo" (palette indices: 0 transparent, 1..4 colors).
    Nothofagus::IndirectTexture makeLogo(const Nothofagus::ColorPallete& pallete)
    {
        Nothofagus::IndirectTexture tex({16, 16}, glm::vec4(0.0f));
        tex.setPallete(pallete);
        std::vector<std::uint8_t> px(16 * 16, 0);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const int d = std::abs(x - 8) + std::abs(y - 8);   // diamond distance
                if (d < 8)
                    px[y * 16 + x] = static_cast<std::uint8_t>(1 + (d % 4));
            }
        tex.setPixels(px, 0);
        return tex;
    }

    // A 16x16, 4-frame animated "spinner": a lit arm sweeping around the center.
    constexpr std::size_t kSpinFrames = 4;
    Nothofagus::IndirectTexture makeSpinner(const Nothofagus::ColorPallete& pallete)
    {
        Nothofagus::IndirectTexture tex({16, 16}, glm::vec4(0.0f), kSpinFrames);
        tex.setPallete(pallete);
        for (std::size_t frame = 0; frame < kSpinFrames; ++frame)
        {
            std::vector<std::uint8_t> px(16 * 16, 0);
            for (int t = 0; t < 7; ++t)   // a radial arm
            {
                const float angle = static_cast<float>(frame) * 1.5707963f; // 90 deg/frame
                const int x = 8 + static_cast<int>(std::round(std::cos(angle) * t));
                const int y = 8 + static_cast<int>(std::round(std::sin(angle) * t));
                if (x >= 0 && x < 16 && y >= 0 && y < 16)
                    px[y * 16 + x] = static_cast<std::uint8_t>(1 + (t % 4));
            }
            tex.setPixels(px, frame);
        }
        return tex;
    }
}

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
        spdlog::info("Markdown link / image clicked: {}", url);
    });

    // Inline images: register the document's images up front (so they are warm-up-free),
    // then map each markdown `src` token to its ImguiImageId via the resolver. They are
    // registered at a generous size; the renderer fits each to the content width per frame.
    const Nothofagus::ColorPallete pallete{
        {0.0f, 0.0f, 0.0f, 0.0f},  // 0: transparent
        {1.0f, 0.4f, 0.4f, 1.0f},  // 1: red
        {0.4f, 1.0f, 0.5f, 1.0f},  // 2: green
        {0.5f, 0.7f, 1.0f, 1.0f},  // 3: blue
        {1.0f, 0.9f, 0.4f, 1.0f},  // 4: yellow
    };
    const Nothofagus::TextureId logoTexId = canvas.addTexture(makeLogo(pallete));
    const Nothofagus::TextureId spinTexId = canvas.addTexture(makeSpinner(pallete));

    const Nothofagus::ImguiImageId logoImageId =
        canvas.registerImguiImage(Nothofagus::Visual{logoTexId}, Nothofagus::ImguiImageSize::Scaled{glm::vec2(6.0f)}); // 16 -> 96px
    const Nothofagus::ImguiImageId spinImageId =
        canvas.registerImguiImage(Nothofagus::Visual{spinTexId}, Nothofagus::ImguiImageSize::Scaled{glm::vec2(6.0f)});

    markdown.setImageResolver([&](std::string_view src) -> std::optional<Nothofagus::ImguiImageId> {
        if (src == "logo")    return logoImageId;
        if (src == "spinner") return spinImageId;
        return std::nullopt;   // unknown src -> dimmed [src] placeholder
    });

    static constexpr const char* kSample = R"md(# Hello Markdown

![logo](logo)

Welcome to **Nothofagus** markdown rendering. This panel shows
*emphasis*, **strong**, ***both***, and `inline code`.

## Inline images

Images resolve to engine sprites via `setImageResolver`. The animated spinner
![spinner](spinner) is a live, paletted, multi-frame texture drawn inline.
An unresolved source renders a placeholder: ![missing](unknown-asset).

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

    float elapsedMs = 0.0f;

    canvas.run([&](float dt)
    {
        // Keep the inline spinner animating: advance its layer and push it onto the
        // registered image (stable id, same size -> no re-warm).
        elapsedMs += dt;
        const std::size_t frame = static_cast<std::size_t>(elapsedMs / 150.0f) % kSpinFrames;
        Nothofagus::Visual spinVisual{spinTexId};
        spinVisual.currentLayer() = frame;
        canvas.updateImguiImage(spinImageId, spinVisual);

        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(640.0f, 600.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Readme");
        markdown.print(kSample);
        ImGui::End();
    });

    return 0;
}
