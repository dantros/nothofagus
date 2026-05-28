#include <nothofagus.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

int main()
{
    Nothofagus::Canvas canvas({320, 240}, "Hello Markdown", {0.10f, 0.10f, 0.14f}, 3);

    // Bake the typeset from the canvas's embedded TTF (Roboto). The bake API
    // dedupes by (sourceId, sizePx), so bold/italic share the body font at
    // 16 px — they are visually identical here. To get true bold/italic
    // glyphs, register a dedicated bold or italic TTF via
    // canvas.addImguiFontSource(...) and bake it at the same size, then
    // point style.bold / style.italic at the new ids. Heading levels use
    // distinct sizes, which is the main visual differentiator.
    const Nothofagus::ImguiFontSourceId source = canvas.defaultImguiFontSourceId();
    Nothofagus::ImguiFontId bodyId = canvas.bakeImguiFont(source, 16.0f);
    Nothofagus::ImguiFontId codeId = canvas.bakeImguiFont(source, 14.0f);
    Nothofagus::ImguiFontId h1Id   = canvas.bakeImguiFont(source, 28.0f);
    Nothofagus::ImguiFontId h2Id   = canvas.bakeImguiFont(source, 22.0f);
    Nothofagus::ImguiFontId h3Id   = canvas.bakeImguiFont(source, 18.0f);

    Nothofagus::MarkdownStyle style;
    style.regular     = bodyId;
    style.bold        = bodyId;
    style.italic      = bodyId;
    style.code        = codeId;
    style.headings[0] = h1Id;
    style.headings[1] = h2Id;
    style.headings[2] = h3Id;

    Nothofagus::MarkdownRenderer markdown(canvas);
    markdown.setStyle(style);
    markdown.setOpenUrlCallback([](std::string_view url) {
        spdlog::info("Markdown link clicked: {}", url);
    });

    static constexpr const char* kSample = R"md(# Hello Markdown

Welcome to **Nothofagus** markdown rendering. This panel shows
*emphasis*, **strong**, ***both***, and `inline code`.

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
| inline images | no (v2)   |

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
