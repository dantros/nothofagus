#include <nothofagus.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

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
