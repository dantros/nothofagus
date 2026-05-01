#include <nothofagus.h>
#include <imgui.h>
#include <cmath>

int main()
{
    Nothofagus::Canvas canvas({256, 224}, "Hello ImGui RTT", {0.10f, 0.10f, 0.14f}, 5);

    // Render target large enough to host a small ImGui panel.
    constexpr Nothofagus::ScreenSize renderTargetSize{160, 120};
    Nothofagus::RenderTargetId renderTargetId = canvas.addRenderTarget(renderTargetSize);
    canvas.setRenderTargetClearColor(renderTargetId, {0.02f, 0.04f, 0.12f, 1.0f});
    Nothofagus::TextureId renderTargetTextureId = canvas.renderTargetTexture(renderTargetId);

    // Display bellota — samples the RTT and shows it in the center of the canvas,
    // slowly rotating so it is visually clear the ImGui content lives inside a texture.
    Nothofagus::BellotaId displayBellotaId =
        canvas.addBellota({{{96.0f, 80.0f}}, renderTargetTextureId});

    float sliderValue = 0.42f;
    bool  checkOn     = true;
    int   clickCount  = 0;
    float time        = 0.0f;

    // When true, the diegetic panel includes an extra "Extra 16 px" line.
    // Toggled by Bake/Remove buttons in the main UI panel; exercises the
    // deferred-bake + atlas-rebuild path on every press.
    bool wantSmall16  = false;

    canvas.run([&](float deltaTimeMS)
    {
        time += deltaTimeMS;

        // Rotate the display bellota around its center.
        canvas.bellota(displayBellotaId).transform().angle() = 0.0005f * time;

        // Queue the ImGui content to be rendered into the RTT this frame.
        canvas.renderImguiTo(renderTargetId, [&]
        {
            // Look up the 12 px diegetic font fresh each frame — atlas
            // rebuilds (triggered by removeImguiFont below) invalidate any
            // previously held ImFont*. Cache hit is O(1).
            ImFont* diegeticFont = canvas.bakeImguiFont(12.0f);
            if (diegeticFont) ImGui::PushFont(diegeticFont);

            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(
                static_cast<float>(renderTargetSize.width),
                static_cast<float>(renderTargetSize.height)), ImGuiCond_Always);

            const ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

            ImGui::Begin("In-World Panel", nullptr, flags);
            ImGui::Text("Diegetic UI");
            ImGui::Separator();
            ImGui::SliderFloat("value", &sliderValue, 0.0f, 1.0f);
            ImGui::Checkbox("enabled", &checkOn);
            if (ImGui::Button("click")) clickCount++;
            ImGui::SameLine();
            ImGui::Text("= %d", clickCount);
            ImGui::ProgressBar(0.5f + 0.5f * std::sin(0.003f * time));

            // Conditional 16 px section — bake-on-demand. First frame after
            // the user clicks "Bake 16 px" returns nullptr (deferred); the
            // frame after, the cache hit returns the new pointer.
            if (wantSmall16)
            {
                ImFont* small16 = canvas.bakeImguiFont(16.0f);
                if (small16)
                {
                    ImGui::PushFont(small16);
                    ImGui::Text("Extra 16 px");
                    ImGui::PopFont();
                }
                else
                {
                    ImGui::Text("(16 px baking...)");
                }
            }
            ImGui::End();

            if (diegeticFont) ImGui::PopFont();
        });

        // Main-canvas ImGui — proves the main context is unaffected.
        // Drag this window around to confirm that input still reaches the main
        // context while the RTT secondary context is active.
        ImGui::SetNextWindowPos(ImVec2(4.0f, 4.0f), ImGuiCond_Once);
        ImGui::Begin("main", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("slider=%.2f  count=%d", sliderValue, clickCount);

        // Stress the atlas remove + rebuild path. Bake schedules a deferred
        // bake; Remove schedules a deferred Clear+rebuild that invalidates
        // every prior ImFont* (the diegetic 12 px font is re-fetched per
        // frame inside the RTT callback to survive this).
        if (ImGui::Button("Bake 16 px font")) wantSmall16 = true;
        ImGui::SameLine();
        if (ImGui::Button("Remove 16 px font"))
        {
            wantSmall16 = false;
            canvas.removeImguiFont(16.0f);
        }
        ImGui::End();
    });

    return 0;
}
