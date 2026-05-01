#include <nothofagus.h>
#include <imgui.h>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <vector>

// Read every byte of `path` into a vector. Returns empty when the file is
// missing or unreadable so the example can run without the optional asset.
static std::vector<std::byte> readFileBytes(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::vector<char> raw((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i)
        bytes[i] = static_cast<std::byte>(raw[i]);
    return bytes;
}

int main()
{
    Nothofagus::Canvas canvas({256, 224}, "Hello Custom Font", {0.10f, 0.10f, 0.14f}, 5);

    // Source from the canvas's built-in default TTF (embedded Roboto). Always
    // available — no filesystem dependency, no glyph-range plumbing.
    Nothofagus::ImguiFontSourceId defaultSrc = canvas.defaultImguiFontSourceId();
    Nothofagus::ImguiFontId default18 = canvas.bakeImguiFont(defaultSrc, 18.0f);

    // Optional user-supplied TTF. Drop a file at `font.ttf` next to the
    // executable to demo the multi-source path; otherwise the example
    // gracefully falls back to the default source.
    std::vector<std::byte> userTtfBytes = readFileBytes("font.ttf");
    bool                          haveUserSrc = !userTtfBytes.empty();
    Nothofagus::ImguiFontSourceId userSrc{};
    Nothofagus::ImguiFontId       user14{};
    if (haveUserSrc)
    {
        userSrc = canvas.addImguiFontSource(userTtfBytes, Nothofagus::GlyphRange::Default);
        user14  = canvas.bakeImguiFont(userSrc, 14.0f);
    }

    // Mid-loop registrations stress the deferred-bake + atlas-rebuild path.
    Nothofagus::ImguiFontId midLoopId{};
    bool                    haveMidLoop = false;

    canvas.run([&](float)
    {
        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
        ImGui::Begin("custom font demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        canvas.pushImguiFont(default18);
        ImGui::Text("Default source @ 18 px");
        canvas.popImguiFont();

        if (haveUserSrc)
        {
            if (canvas.isImguiFontReady(user14))
            {
                canvas.pushImguiFont(user14);
                ImGui::Text("User source @ 14 px");
                canvas.popImguiFont();
            }
            else
            {
                ImGui::Text("(user font baking...)");
            }
        }
        else
        {
            ImGui::TextDisabled("Drop a font.ttf next to the executable to load a user TTF.");
        }

        ImGui::Separator();

        // Mid-loop: register a NEW source from the same bytes (or, when no
        // user TTF is available, re-derive from default by baking a fresh
        // size). Demonstrates the deferred-bake path inside the update loop.
        if (ImGui::Button("Bake mid-loop") && !haveMidLoop)
        {
            midLoopId = haveUserSrc
                ? canvas.bakeImguiFont(userSrc, 22.0f)
                : canvas.bakeImguiFont(defaultSrc, 22.0f);
            haveMidLoop = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove mid-loop") && haveMidLoop)
        {
            canvas.removeImguiFont(midLoopId);
            haveMidLoop = false;
        }

        if (haveMidLoop)
        {
            if (canvas.isImguiFontReady(midLoopId))
            {
                canvas.pushImguiFont(midLoopId);
                ImGui::Text("Mid-loop @ 22 px");
                canvas.popImguiFont();
            }
            else
            {
                ImGui::Text("(mid-loop baking...)");
            }
        }

        // Cascade-remove the user source: invalidates user14 (and midLoopId
        // when it's attributed to userSrc) on the next frame's drain.
        if (haveUserSrc && ImGui::Button("Remove user source"))
        {
            canvas.removeImguiFontSource(userSrc);
            haveUserSrc = false;
            haveMidLoop = false;     // midLoopId may have been baked from userSrc
        }

        ImGui::End();
    });

    return 0;
}
