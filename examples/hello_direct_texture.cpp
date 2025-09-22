#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>

int main()
{
    // You can directly use spdlog to ease your logging
    spdlog::info("Hello Direct Texture!");

    Nothofagus::Canvas canvas({150, 100}, "Hello Direct Texture!", {0.7, 0.7, 0.7}, 6);

    Nothofagus::DirectTexture texture1({ 5, 5 }, { 0.5, 0.5, 0.5, 1.0 });
    Nothofagus::TextureId textureId1 = canvas.addTexture(texture1);  //WIP

    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{10.0f, 10.0f}}, textureId1});
    Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{20.0f, 10.0f}}, textureId1});

    float time = 0.0f;
    bool rotate = true;
    bool visible = true;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota1 = canvas.bellota(bellotaId1);

        Nothofagus::Bellota& bellota2 = canvas.bellota(bellotaId2);
        bellota2.transform().location().x = 75.0f + 60.0f * std::sin(0.0005f * time);

        // you can directly use ImGui
        ImGui::Begin("Hello there!");
        ImGui::Text("May ImGui be with you...");
        ImGui::Checkbox("Rotate?", &rotate);
        if (rotate)
        {
            bellota2.transform().angle() = 0.1f * time;
        }
        ImGui::Checkbox("Visible?", &visible);
        bellota1.visible() = visible;
        ImGui::End();
    };
    
    canvas.run(update);
    
    return 0;
}