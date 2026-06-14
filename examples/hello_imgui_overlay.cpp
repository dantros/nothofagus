#include <string>
#include <nothofagus.h>

// Header / footer overlay bars pinned to the game canvas via the public overlay
// API. Resize or fullscreen the window: the bars keep tracking the
// pillarboxed/letterboxed canvas and the text stays centered, on any backend
// (GLFW/SDL3 x OpenGL/Vulkan) and at any OS DPI / contentScale. This is the
// reference for the engine_header / engine_footer Python overlays.
//
// Two pieces do the work:
//   canvas.imguiOverlayViewport() -> the game viewport in ImGui display points
//                                    (top-left origin), already DPI-converted.
//   canvas.imguiScaledFontSize()  -> the DPI-scaled base font size, so the bar
//                                    grows with the standard UI on HiDPI and the
//                                    bar/text proportion stays constant.
static void drawOverlayBars(Nothofagus::Canvas& canvas,
                            const std::string& headerText,
                            const std::string& footerText)
{
    const Nothofagus::ImguiOverlayRect rect = canvas.imguiOverlayViewport();
    const float barHeight = canvas.imguiScaledFontSize() * 1.875f;

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    auto drawBar = [&](const char* id, float yTop, const std::string& text)
    {
        ImGui::SetNextWindowPos(ImVec2(rect.x, yTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(rect.width, barHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        // Bars are thinner than ImGui's default WindowMinSize (32 px, and
        // 32 * contentScale on HiDPI); without this the windows inflate to that
        // minimum — the top header shows the full inflated height while the
        // bottom footer's surplus is clipped off-screen, so they look uneven.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
        ImGui::Begin(id, nullptr, flags);
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::SetCursorPos(ImVec2((rect.width - textSize.x) * 0.5f, (barHeight - textSize.y) * 0.5f));
        ImGui::TextUnformatted(text.c_str());
        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(1);
    };

    drawBar("##overlay_header", rect.y, headerText);
    drawBar("##overlay_footer", rect.y + rect.height - barHeight, footerText);
}

int main()
{
    spdlog::info("Hello ImGui overlay!");

    Nothofagus::Canvas canvas({200, 150}, "Hello ImGui Overlay", {0.15f, 0.18f, 0.28f}, 4);

    // A sprite in the middle so the pillarbox/letterbox bands are obvious when
    // the window is resized.
    Nothofagus::ColorPallete pallete{
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.8f, 0.2f, 1.0f},
        {0.9f, 0.3f, 0.2f, 1.0f},
    };
    Nothofagus::IndirectTexture texture({4, 4}, {0.0f, 0.0f, 0.0f, 0.0f});
    texture.setPallete(pallete).setPixels({
        1, 2, 2, 1,
        2, 1, 1, 2,
        2, 1, 1, 2,
        1, 2, 2, 1,
    });
    const Nothofagus::TextureId textureId = canvas.addTexture(texture);
    canvas.addBellota({{{100.0f, 75.0f}, 6.0f}, textureId});

    auto update = [&](float)
    {
        drawOverlayBars(canvas,
                        "HEADER - resize the window; the bar tracks the canvas",
                        "FOOTER - imguiOverlayViewport() + imguiBaseFontSize()");
    };

    canvas.run(update);
    return 0;
}
