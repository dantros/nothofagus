#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
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

    Nothofagus::Texture texture1({ 4, 4 }, { 0.5, 0.5, 0.5, 1.0 });
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
    
    Nothofagus::Texture texture2({8, 8}, {0.5, 0.5, 0.5, 1.0});
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
    bool rotate = true;
    bool visible = true;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota2 = canvas.bellota(bellotaId2);
        bellota2.transform().location().x = 75.0f + 60.0f * std::sin(0.0005f * time);

        Nothofagus::Bellota& bellota3 = canvas.bellota(bellotaId3);

        // you can directly use ImGui
        ImGui::Begin("Hello there!");
        ImGui::Text("May ImGui be with you...");
        ImGui::Checkbox("Rotate?", &rotate);
        if (rotate)
        {
            bellota3.transform().angle() = 0.1f * time;
        }
        ImGui::Checkbox("Visible?", &visible);
        bellota3.visible() = visible;
        ImGui::End();
    };
    
    canvas.run(update);
    
    return 0;
}