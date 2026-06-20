// hello_threaded — two-thread sim/render split (M2 Phase A).
//
// The simulation runs on its own std::thread, animating a field of sprites and
// committing a frame snapshot each tick. The main thread renders the previous
// snapshot, so render of frame N overlaps simulation of frame N+1.
//
// nothofagus spawns no threads: this file owns both loops. Per the Phase-A
// contract, every texture/bellota is created up front and the simulation only
// mutates existing bellota *values* (transform) at runtime — no resource
// create/destroy and no ImGui on the threaded path.

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>
#include <nothofagus.h>

int main()
{
    spdlog::info("hello_threaded: two-thread sim/render split");

    Nothofagus::ScreenSize screenSize{200, 150};
    Nothofagus::Canvas canvas(screenSize, "Hello Threaded", {0.05f, 0.05f, 0.10f}, 5);

    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},
        {0.9, 0.3, 0.2, 1.0},
        {1.0, 0.8, 0.2, 1.0},
        {0.3, 0.7, 1.0, 1.0},
    };

    Nothofagus::IndirectTexture texture({8, 8}, {0.0, 0.0, 0.0, 0.0});
    texture.setPallete(pallete)
        .setPixels(
        {
            0,0,1,1,1,1,0,0,
            0,1,2,2,2,2,1,0,
            1,2,2,3,3,2,2,1,
            1,2,3,3,3,3,2,1,
            1,2,3,3,3,3,2,1,
            1,2,2,3,3,2,2,1,
            0,1,2,2,2,2,1,0,
            0,0,1,1,1,1,0,0,
        });
    const Nothofagus::TextureId textureId = canvas.addTexture(texture);

    // --- All resources created up front (Phase A): a grid of sprites. ---
    struct Sprite
    {
        Nothofagus::BellotaId id;
        glm::vec2 home;
        float phase;
    };
    std::vector<Sprite> sprites;

    constexpr int columns = 9;
    constexpr int rows = 7;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            const float x = 20.0f + col * 20.0f;
            const float y = 20.0f + row * 18.0f;
            const Nothofagus::BellotaId id = canvas.addBellota({{{x, y}, 1.0f}, textureId});
            sprites.push_back({id, glm::vec2(x, y), 0.35f * (col + row)});
        }
    }

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.close();
    });

    // --- Simulation: animate sprite transforms (existing bellota values only). ---
    float simTime = 0.0f;
    auto update = [&](float dt)
    {
        simTime += dt;
        for (const Sprite& sprite : sprites)
        {
            Nothofagus::Bellota& bellota = canvas.bellota(sprite.id);
            const float wave = 0.0025f * simTime + sprite.phase;
            bellota.transform().location().x = sprite.home.x + 8.0f * std::sin(wave);
            bellota.transform().location().y = sprite.home.y + 8.0f * std::cos(wave * 0.7f);
            bellota.transform().scale() = glm::vec2(1.0f + 0.4f * std::sin(wave * 1.3f));
            bellota.transform().angle() += 0.05f * dt;
        }
    };

    canvas.beginThreadedSession(controller);

    // Simulation thread: commit at ~120 Hz, decoupled from the render cadence.
    std::thread simThread([&]()
    {
        using clock = std::chrono::steady_clock;
        auto previous = clock::now();
        constexpr auto targetPeriod = std::chrono::microseconds(8333); // ~120 Hz
        while (canvas.isThreadedRunning())
        {
            const auto now = clock::now();
            const float dt = std::chrono::duration<float, std::milli>(now - previous).count();
            previous = now;

            canvas.commit(dt, update);

            std::this_thread::sleep_for(targetPeriod);
        }
    });

    // Main thread: render the latest committed snapshot + pump window/input.
    while (canvas.isThreadedRunning())
    {
        canvas.renderFrame(controller);
    }

    simThread.join();
    return 0;
}
