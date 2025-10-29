#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <algorithm>
#include <nothofagus.h>

int main()
{
    Nothofagus::ScreenSize screenSize{150, 100};
    Nothofagus::Canvas canvas(screenSize, "Keyboard test", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.4, 0.0, 1.0},
        {0.2, 0.8, 0.2, 1.0},
        {0.5, 1.0, 0.5, 1.0},
    };
    
    Nothofagus::IndirectTexture texture({8, 8}, {0.5, 0.5, 0.5, 1.0});
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
    constexpr float growingSpeed = 0.1;
    constexpr float horizontalSpeed = 0.1;
    bool rotate = true;
    bool leftKeyPressed = false;
    bool rightKeyPressed = false;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);
        float scale = 2.0f + 0.5f * std::sin(0.005f * time);
        bellota.transform().scale() = glm::vec2(scale, scale);

        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Hello there!");
        ImGui::Text("Discrete control keys: W, S, ESCAPE");
        ImGui::Text("Continuous control keys: A, D");
        ImGui::Text("Show/hide performance stats: Q");
        ImGui::End();

        if (rotate)
            bellota.transform().angle() += angularSpeed * dt;

        if (leftKeyPressed)
            bellota.transform().location().x -= horizontalSpeed * dt;

        if (rightKeyPressed)
            bellota.transform().location().x += horizontalSpeed * dt;

        bellota.transform().location().x = std::clamp<float>(bellota.transform().location().x, 10, screenSize.width-10);
    };

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.bellota(bellotaId).transform().location().y += 10.0f;
    });
    controller.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.bellota(bellotaId).transform().location().y -= 10.0f;
    });
    controller.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        leftKeyPressed = true;
    });
    controller.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Release}, [&]()
    {
        leftKeyPressed = false;
    });
    controller.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        rightKeyPressed = true;
    });
    controller.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Release}, [&]()
    {
        rightKeyPressed = false;
    });
    controller.registerAction({ Nothofagus::Key::Q, Nothofagus::DiscreteTrigger::Press }, [&]()
    {
        canvas.stats() = not canvas.stats();
    });
    controller.registerAction({Nothofagus::Key::SPACE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        rotate = not rotate;
    });
    controller.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press}, [&]() { canvas.close(); });
    
    canvas.run(update, controller);
    
    return 0;
}