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

    // Bake the 12 px diegetic font once at startup, from the canvas's built-in
    // default source (embedded Roboto). The returned id is stable across atlas
    // rebuilds — only removeImguiFont(diegeticId) invalidates it.
    Nothofagus::ImguiFontId diegeticId =
        canvas.bakeImguiFont(canvas.defaultImguiFontSourceId(), 12.0f);

    float sliderValue = 0.42f;
    bool  checkOn     = true;
    int   clickCount  = 0;
    float time        = 0.0f;

    // 16 px font, optional. Created on Bake button click; removed on Remove.
    // wantSmall16 gates the diegetic-panel section that uses it; small16Id is
    // valid only while wantSmall16 is true.
    Nothofagus::ImguiFontId small16Id{};
    bool wantSmall16 = false;

    canvas.run([&](float deltaTimeMS)
    {
        time += deltaTimeMS;

        // Rotate the display bellota around its center.
        canvas.bellota(displayBellotaId).transform().angle() = 0.0005f * time;

        // Queue the ImGui content to be rendered into the RTT this frame.
        // The id-bearing renderImguiTo overload auto-pushes diegeticId for
        // the duration of the callback (no ImFont* in user code, falls back
        // to the secondary-context default if the bake is still pending).
        canvas.renderImguiTo(renderTargetId, diegeticId, [&]
        {
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

            // Mid-callback font override via the id-based push/pop helpers.
            // isImguiFontReady is false during the one-frame deferred-bake
            // window after the Bake button click.
            if (wantSmall16)
            {
                if (canvas.isImguiFontReady(small16Id))
                {
                    canvas.pushImguiFont(small16Id);
                    ImGui::Text("Extra 16 px");
                    canvas.popImguiFont();
                }
                else
                {
                    ImGui::Text("(16 px baking...)");
                }
            }
            ImGui::End();
        });

        // Main-canvas ImGui — proves the main context is unaffected.
        // Drag this window around to confirm that input still reaches the main
        // context while the RTT secondary context is active.
        ImGui::SetNextWindowPos(ImVec2(4.0f, 4.0f), ImGuiCond_Once);
        ImGui::Begin("main", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("slider=%.2f  count=%d", sliderValue, clickCount);

        // Stress the atlas remove + rebuild path. Bake captures a stable id;
        // Remove invalidates only that id — diegeticId keeps working across
        // every rebuild because the cache patches its underlying ImFont* in
        // place during rebakeAll().
        if (ImGui::Button("Bake 16 px font"))
        {
            small16Id   = canvas.bakeImguiFont(canvas.defaultImguiFontSourceId(), 16.0f);
            wantSmall16 = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove 16 px font") && wantSmall16)
        {
            canvas.removeImguiFont(small16Id);
            wantSmall16 = false;
        }
        ImGui::End();
    });

    return 0;
}
