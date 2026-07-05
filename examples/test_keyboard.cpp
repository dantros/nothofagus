#include <iostream>
#include <string>
#include <vector>
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

    // Game logic — runs on the sim thread (no ImGui here).
    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);
        float scale = 2.0f + 0.5f * std::sin(0.005f * time);
        bellota.transform().scale() = glm::vec2(scale, scale);

        if (rotate)
            bellota.transform().angle() += angularSpeed * dt;

        if (leftKeyPressed)
            bellota.transform().location().x -= horizontalSpeed * dt;

        if (rightKeyPressed)
            bellota.transform().location().x += horizontalSpeed * dt;

        bellota.transform().location().x = std::clamp<float>(bellota.transform().location().x, 10, screenSize.width-10);
    };

    // ImGui — runs on the sim-UI context (sim thread), cloned to the render thread.
    auto ui = [&](float)
    {
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Hello there!");
        ImGui::Text("Discrete control keys: W, S, ESCAPE");
        ImGui::Text("Continuous control keys: A, D");
        ImGui::Text("Show/hide performance stats: Q");
        ImGui::Text("Toggle fullscreen/windowed in the current monitor: F");
        ImGui::End();
    };

    // Game input — dispatched on the sim thread; mutates the live scene / sim state.
    Nothofagus::Controller simController;
    simController.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.bellota(bellotaId).transform().location().y += 10.0f;
    });
    simController.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.bellota(bellotaId).transform().location().y -= 10.0f;
    });
    simController.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        leftKeyPressed = true;
    });
    simController.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Release}, [&]()
    {
        leftKeyPressed = false;
    });
    simController.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        rightKeyPressed = true;
    });
    simController.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Release}, [&]()
    {
        rightKeyPressed = false;
    });
    simController.registerAction({ Nothofagus::Key::Q, Nothofagus::DiscreteTrigger::Press }, [&]()
    {
        canvas.stats() = not canvas.stats();
    });
    simController.registerAction({Nothofagus::Key::SPACE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        rotate = not rotate;
    });

    // Window input — dispatched on the main thread; window/monitor ops + close.
    Nothofagus::Controller renderController;
    renderController.registerAction({ Nothofagus::Key::F, Nothofagus::DiscreteTrigger::Press }, [&]()
    {
        if (canvas.isFullscreen())
        {
            canvas.setWindowed();
        }
        else
        {
            std::size_t monitor = canvas.getCurrentMonitor();
            canvas.setFullScreenOnMonitor(monitor);
        }
    });
    renderController.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press}, [&]() { canvas.close(); });

    // Multithreaded convenience: sim thread runs update + ui + simController; the
    // main thread runs the render loop + renderController.
    canvas.run(update, ui, simController, renderController);

    return 0;
}