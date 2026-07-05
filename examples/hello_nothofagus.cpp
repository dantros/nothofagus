#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <nothofagus.h>

int main()
{
    // You can directly use spdlog to ease your logging
    spdlog::info("Hello Nothofagus!");

    Nothofagus::Canvas canvas({150, 100}, "Hello Nothofagus", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete pallete1{
        {0.0, 0.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0},
        {1.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 0.0}
    };

    Nothofagus::ColorPallete pallete2{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.4, 0.0, 1.0},
        {0.2, 0.8, 0.2, 1.0},
        {0.5, 1.0, 0.5, 1.0},
    };

    Nothofagus::IndirectTexture texture1({ 4, 4 }, { 0.5, 0.5, 0.5, 1.0 });
    texture1.setPallete(pallete1)
    .setPixels(
        {
            0,1,2,3,
            4,5,6,7,
            0,1,2,3,
            4,5,6,7
        }
    );    
    Nothofagus::TextureId textureId1 = canvas.addTexture(texture1);
    
    Nothofagus::IndirectTexture texture2({8, 8}, {0.5, 0.5, 0.5, 1.0});
    texture2.setPallete(pallete2)
        .setPixels(
        {
            2,1,3,0,0,3,2,1,
            2,1,1,0,0,0,2,1,
            2,1,1,1,0,0,2,1,
            2,1,2,1,1,0,2,1,
            2,1,0,2,1,1,2,1,
            2,1,0,0,2,1,2,1,
            2,1,0,0,0,2,2,1,
            2,1,3,0,0,3,2,1,
        }
    );
    Nothofagus::TextureId textureId2 = canvas.addTexture(texture2);

    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{10.0f, 10.0f}}, textureId1});
    Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{20.0f, 10.0f}}, textureId1});
    Nothofagus::BellotaId bellotaId3 = canvas.addBellota({ {{50.0f, 50.0f}, 4.0}, textureId2 });
    Nothofagus::BellotaId bellotaId4 = canvas.addBellota({ {{100.0f, 50.0f}, 2.0}, textureId2 });

    float time = 0.0f;
    // Shared between update and ui — both run on the sim thread, so plain bools are safe
    // (ui writes them, update reads them; no atomics needed).
    bool rotate = true;
    bool visible = true;

    // Game logic — runs on the sim thread (no ImGui here).
    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota2 = canvas.bellota(bellotaId2);
        bellota2.transform().location().x = 75.0f + 60.0f * std::sin(0.0005f * time);

        Nothofagus::Bellota& bellota3 = canvas.bellota(bellotaId3);
        if (rotate)
            bellota3.transform().angle() = 0.1f * time;
        bellota3.visible() = visible;
    };

    // Interactive ImGui — runs on the sim-UI context (sim thread), cloned to the render
    // thread. Widgets respond to the mouse (input is marshaled render -> sim).
    auto ui = [&](float)
    {
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Hello there!");
        ImGui::Text("May ImGui be with you...");
        ImGui::Checkbox("Rotate?", &rotate);
        ImGui::Checkbox("Visible?", &visible);
        ImGui::End();
    };

    // Multithreaded convenience: the sim thread runs update + ui; the main thread runs
    // the render loop. No controllers — this demo closes via the window's X button.
    canvas.run(update, ui);

    return 0;
}