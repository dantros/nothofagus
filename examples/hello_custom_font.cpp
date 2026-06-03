#include <nothofagus.h>
#include <imgui.h>
#include <imfilebrowser.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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

    Nothofagus::ImguiFontId default18 =
        canvas.bakeImguiFont(canvas.defaultImguiFontSourceId(), 18.0f);

    // Embedded CJK fonts (opt-in). Each block compiles in only when its
    // NOTHOFAGUS_EMBED_CJK_* CMake option is ON (which the example target
    // mirrors as a NOTHOFAGUS_HAS_CJK_* define). embeddedCjkFontSource is
    // guaranteed non-null in that config, so we deref directly. Baking before
    // run() is synchronous (the atlas is unlocked), so these are ready
    // immediately. Plain string literals are UTF-8 under clang/clang-cl.
    struct CjkDemo { const char* label; const char* sample; Nothofagus::ImguiFontId font; };
    std::vector<CjkDemo> cjkDemos;
    [[maybe_unused]] auto bakeCjk = [&](Nothofagus::CjkScript script) {
        return canvas.bakeImguiFont(*canvas.embeddedCjkFontSource(script), 28.0f);
    };
#ifdef NOTHOFAGUS_HAS_CJK_SC
    cjkDemos.push_back({ "Simplified Chinese", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87\xe7\xa4\xba\xe4\xbe\x8b",
                         bakeCjk(Nothofagus::CjkScript::SimplifiedChinese) });   // 简体中文示例
#endif
#ifdef NOTHOFAGUS_HAS_CJK_TC
    cjkDemos.push_back({ "Traditional Chinese", "\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87\xe7\xaf\x84\xe4\xbe\x8b",
                         bakeCjk(Nothofagus::CjkScript::TraditionalChinese) });  // 繁體中文範例
#endif
#ifdef NOTHOFAGUS_HAS_CJK_JP
    cjkDemos.push_back({ "Japanese", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x82\xb5\xe3\x83\xb3\xe3\x83\x97\xe3\x83\xab",
                         bakeCjk(Nothofagus::CjkScript::Japanese) });            // 日本語のサンプル
#endif
#ifdef NOTHOFAGUS_HAS_CJK_KR
    cjkDemos.push_back({ "Korean", "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4 \xec\x98\x88\xec\x8b\x9c",
                         bakeCjk(Nothofagus::CjkScript::Korean) });              // 한국어 예시
#endif

    char pathBuf[512] = "";
    char textBuf[512] = "The quick brown fox jumps over the lazy dog";
    int  fontSize     = 24;
    int  sizeMin      = 8;
    int  sizeMax      = 100;

    Nothofagus::ImguiFontSourceId userSrc{};
    Nothofagus::ImguiFontId       user{};
    bool                          fontLoaded = false;
    std::string                   lastError;

    ImGui::FileBrowser fontDialog;
    fontDialog.SetTitle("Choose a font file");
    fontDialog.SetTypeFilters({ ".ttf", ".otf" });

    canvas.run([&](float)
    {
        bool commitPath = false;

        ImGui::SetNextWindowSize(ImVec2(420.f, 360.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("custom font demo");

        // Commit the path on Enter inside the field OR by clicking Load (so
        // paste-then-click works) OR by picking a file in the Browse dialog.
        // On commit: cascade-remove the old source (if any), register the new
        // bytes as a source, bake at the current size. Atlas is locked here,
        // so the bake is deferred — the next frame's drain runs RemoveSource
        // + atlas Clear + rebake.
        if (ImGui::InputTextWithHint("font path", "path/to/font.ttf",
                                     pathBuf, sizeof(pathBuf),
                                     ImGuiInputTextFlags_EnterReturnsTrue))
            commitPath = true;
        ImGui::SameLine();
        if (ImGui::Button("Load")) commitPath = true;
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) fontDialog.Open();

        ImGui::InputText("text", textBuf, sizeof(textBuf));

        // Min/max integer fields drive the slider's range. Each is clamped to
        // [1, 1000]; min is then capped at max (and max floored at min) so the
        // interval is always valid even mid-typing. fontSize is reclamped into
        // the current interval before the slider runs, then any net change to
        // fontSize (from min/max editing or from the slider) triggers a rebake.
        const int prevFontSize = fontSize;

        ImGui::InputInt("min (px)", &sizeMin);
        sizeMin = std::clamp(sizeMin, 1, 1000);
        if (sizeMin > sizeMax) sizeMin = sizeMax;

        ImGui::InputInt("max (px)", &sizeMax);
        sizeMax = std::clamp(sizeMax, 1, 1000);
        if (sizeMax < sizeMin) sizeMax = sizeMin;

        fontSize = std::clamp(fontSize, sizeMin, sizeMax);

        ImGui::SliderInt("size (px)", &fontSize, sizeMin, sizeMax);

        if (fontSize != prevFontSize && fontLoaded)
            user = canvas.bakeImguiFont(userSrc, float(fontSize));

        ImGui::Separator();

        canvas.pushImguiFont(default18);
        ImGui::Text("Default font:");
        canvas.popImguiFont();
        ImGui::TextWrapped("%s", textBuf);

        ImGui::Separator();

        canvas.pushImguiFont(default18);
        ImGui::Text("User font:");
        canvas.popImguiFont();

        if (!lastError.empty())
            ImGui::TextDisabled("%s", lastError.c_str());

        if (fontLoaded)
        {
            if (canvas.isImguiFontReady(user))
            {
                canvas.pushImguiFont(user);
                ImGui::TextWrapped("%s", textBuf);
                canvas.popImguiFont();
            }
            else
            {
                ImGui::TextDisabled("(baking %d px...)", fontSize);
            }
        }
        else
        {
            ImGui::TextDisabled("(type a TTF path above and press Enter)");
        }

        if (!cjkDemos.empty())
        {
            ImGui::Separator();
            canvas.pushImguiFont(default18);
            ImGui::Text("CJK (embedded, opt-in):");
            canvas.popImguiFont();
            for (const CjkDemo& demo : cjkDemos)
            {
                ImGui::TextDisabled("%s", demo.label);
                canvas.pushImguiFont(demo.font);
                ImGui::Text("%s", demo.sample);
                canvas.popImguiFont();
            }
        }

        ImGui::End();

        fontDialog.Display();
        if (fontDialog.HasSelected())
        {
            const std::string picked = fontDialog.GetSelected().string();
            const std::size_t n = std::min(picked.size(), sizeof(pathBuf) - 1);
            std::memcpy(pathBuf, picked.data(), n);
            pathBuf[n] = '\0';
            fontDialog.ClearSelected();
            commitPath = true;
        }

        if (commitPath)
        {
            auto bytes = readFileBytes(pathBuf);
            if (bytes.empty())
            {
                lastError = std::string("could not read: ") + pathBuf;
            }
            else
            {
                if (fontLoaded) canvas.removeImguiFontSource(userSrc);
                userSrc    = canvas.addImguiFontSource(bytes, Nothofagus::GlyphRange::Default);
                user       = canvas.bakeImguiFont(userSrc, float(fontSize));
                fontLoaded = true;
                lastError.clear();
            }
        }
    });

    return 0;
}
