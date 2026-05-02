#include <nothofagus.h>
#include <imgui.h>
#include <algorithm>
#include <cstddef>
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

    char pathBuf[512] = "";
    char textBuf[512] = "The quick brown fox jumps over the lazy dog";
    int  fontSize     = 24;
    int  sizeMin      = 8;
    int  sizeMax      = 100;

    Nothofagus::ImguiFontSourceId userSrc{};
    Nothofagus::ImguiFontId       user{};
    bool                          fontLoaded = false;
    std::string                   lastError;

    canvas.run([&](float)
    {
        ImGui::SetNextWindowSize(ImVec2(420.f, 360.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("custom font demo");

        // Commit the path on Enter inside the field OR by clicking Load (so
        // paste-then-click works). On commit: cascade-remove the old source
        // (if any), register the new bytes as a source, bake at the current
        // size. Atlas is locked here, so the bake is deferred — the next
        // frame's drain runs RemoveSource + atlas Clear + rebake.
        bool commitPath = ImGui::InputTextWithHint("font path", "path/to/font.ttf",
                                                    pathBuf, sizeof(pathBuf),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Load")) commitPath = true;

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

        ImGui::End();
    });

    return 0;
}
