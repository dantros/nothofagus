// hello_threaded — two-thread sim/render split (M2).
//
// The simulation runs on its own std::thread; the main thread renders the
// previous frame's snapshot, so render of frame N overlaps simulation of N+1.
// nothofagus spawns no threads — this file owns both loops.
//
// Phase A established the snapshot hand-off with a static scene (the sim only
// mutated existing bellota values). Later phases add runtime structural churn:
// from commit()'s update during a live threaded session the simulation
// continuously creates and destroys resources via the regular Canvas API —
// bellotas (addBellota/removeBellota) with a per-sprite texture (addTexture), and
// a rotating set of render targets (addRenderTarget/removeRenderTarget) — while
// the render thread is a frame behind. Those serialize structural changes against
// the renderer, and the GPU resources a removal frees (the sprite's texture +
// auto-quad mesh, or the RTT's FBO/proxy/ImGui context) are released by the
// render thread only once no in-flight frame still references them — so there is
// no use-after-free despite the one-frame lag.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <thread>
#include <utility>
#include <vector>
#include <nothofagus.h>

namespace
{
// Tiny deterministic-ish LCG so the demo does not pull in <random> and behaves
// the same regardless of platform RNG.
struct Lcg
{
    std::uint32_t state;
    float next01()
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>((state >> 8) & 0xFFFFFFu) / static_cast<float>(0x1000000u);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * next01(); }
};
}

int main()
{
    spdlog::info("hello_threaded: two-thread sim/render with runtime spawn/despawn churn");

    const Nothofagus::ScreenSize screenSize{200, 150};
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
    struct Sprite
    {
        Nothofagus::BellotaId id;
        glm::vec2 home;
        float phase;
        float age;       // ms
        float lifespan;  // ms
    };
    std::vector<Sprite> sprites;   // sim-thread-only ownership
    Lcg rng{0x1234567u};

    constexpr std::size_t targetCount = 60;

    // Each sprite gets its OWN texture, created on the sim thread via addBellota's
    // sibling addTexture and freed (deferred) when its bellota is removed — so
    // textures churn on the sim thread too, not just auto-quad meshes.
    auto spawnOne = [&](float now)
    {
        const float x = rng.range(15.0f, screenSize.width - 15.0f);
        const float y = rng.range(15.0f, screenSize.height - 15.0f);
        const Nothofagus::TextureId textureId = canvas.addTexture(texture);
        const Nothofagus::BellotaId id = canvas.addBellota({{{x, y}, 1.0f}, textureId});
        sprites.push_back({id, glm::vec2(x, y), rng.range(0.0f, 6.28f), 0.0f, rng.range(1500.0f, 4000.0f)});
        (void)now;
    };

    // Render-target churn — create/destroy RTTs from the sim thread to exercise
    // the deferred render-target free (FBO/VkImage + proxy texture + per-RTT ImGui
    // context, all torn down on the render thread once no in-flight frame uses them).
    std::deque<std::pair<Nothofagus::RenderTargetId, float>> scratchRenderTargets;

    float simTime = 0.0f;
    auto update = [&](float dt)
    {
        simTime += dt;

        // Age + animate live sprites; despawn the expired ones (swap-erase).
        for (std::size_t i = 0; i < sprites.size();)
        {
            Sprite& sprite = sprites[i];
            sprite.age += dt;
            if (sprite.age >= sprite.lifespan)
            {
                canvas.removeBellota(sprite.id);
                sprite = sprites.back();
                sprites.pop_back();
                continue;
            }
            Nothofagus::Bellota& bellota = canvas.bellota(sprite.id);
            const float wave = 0.0025f * simTime + sprite.phase;
            bellota.transform().location().x = sprite.home.x + 8.0f * std::sin(wave);
            bellota.transform().location().y = sprite.home.y + 8.0f * std::cos(wave * 0.7f);
            const float life = sprite.age / sprite.lifespan;       // 0..1
            const float fade = std::sin(life * 3.14159f);          // grow then shrink
            bellota.transform().scale() = glm::vec2(0.4f + 1.2f * fade);
            bellota.transform().angle() += 0.05f * dt;
            ++i;
        }

        // Refill toward the target population (a few per frame so churn is steady).
        int budget = 3;
        while (sprites.size() < targetCount && budget-- > 0)
            spawnOne(simTime);

        // Render-target churn: spawn one roughly every 100 ms, retire after ~400 ms.
        if (scratchRenderTargets.empty() || simTime - scratchRenderTargets.back().second > 100.0f)
            scratchRenderTargets.push_back({canvas.addRenderTarget({32, 32}), simTime});
        while (!scratchRenderTargets.empty() && simTime - scratchRenderTargets.front().second > 400.0f)
        {
            canvas.removeRenderTarget(scratchRenderTargets.front().first);
            scratchRenderTargets.pop_front();
        }
    };

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.close();
    });

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
