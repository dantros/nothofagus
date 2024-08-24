#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>

int main()
{
    Nothofagus::Canvas canvas({150, 100}, "Keyboard test", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.4, 0.0, 1.0},
        {0.2, 0.8, 0.2, 1.0},
        {0.5, 1.0, 0.5, 1.0},
    };
    
    Nothofagus::Texture texture({8, 8}, {0.5, 0.5, 0.5, 1.0});
    texture.setPallete(pallete)
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
    Nothofagus::TextureId textureId = canvas.addTexture(texture);
    Nothofagus::BellotaId bellotaId = canvas.addBellota({{{75.0f, 75.0f}}, textureId});

    float time = 0.0f;
    constexpr float angularSpeed = 0.1;
    bool rotate = true;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);
        bellota.transform().location().x = 75.0f + 60.0f * std::sin(0.0005f * time);

        ImGui::Begin("Hello there!");
        ImGui::Text("May ImGui be with you...");
        ImGui::End();

        if (rotate)
            bellota.transform().angle() += angularSpeed * dt;
    };

    Nothofagus::Controller controller;
    controller.onKeyPress(Nothofagus::Key::W, [&]()
    {
        canvas.bellota(bellotaId).transform().location().y += 10.0f;
    });
    controller.onKeyPress(Nothofagus::Key::S, [&]()
    {
        canvas.bellota(bellotaId).transform().location().y -= 10.0f;
    });
    controller.onKeyPress(Nothofagus::Key::SPACE, [&]()
    {
        rotate = not rotate;
    });
    //controller.onKeyPress(Nothofagus::Key::ESCAPE, [&]() { canvas.close(); });
    
    canvas.run(update, controller);
    
    return 0;
}