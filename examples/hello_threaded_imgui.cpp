// hello_threaded_imgui — interactive ImGui on the threaded sim/render split (M3).
//
// Same two-thread model as hello_threaded (sim thread commits; main thread
// renders the previous frame), but now the simulation thread also runs an
// *interactive* ImGui panel inside its update. The widgets execute on the sim
// thread (a dedicated ImGui context), their draw data is deep-cloned into the
// snapshot, and the main thread renders the clone. Mouse input is marshalled
// render→sim, so the sliders/checkbox respond to the cursor while the renderer
// runs a frame behind. No ImGui code runs on two threads at once.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>
#include <nothofagus.h>

namespace
{
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
    spdlog::info("hello_threaded_imgui: interactive ImGui on the sim thread");

    const Nothofagus::ScreenSize screenSize{220, 165};
    Nothofagus::Canvas canvas(screenSize, "Hello Threaded ImGui", {0.05f, 0.06f, 0.11f}, 5);

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

    // Anchor bellota: a single, hidden, never-despawned bellota that holds a
    // permanent reference to the shared sprite texture. The engine auto-GCs any
    // texture left unreferenced by every bellota, and the sprite population can
    // momentarily drop to zero (all sprites aging out, or "clear all"). Without
    // this anchor, the texture would be reclaimed and the next spawn reusing
    // textureId would reference a freed texture. Keeping one hidden bellota alive
    // for the whole program pins textureId. It is intentionally never added to
    // `sprites`, so the despawn/aging logic can never destroy it.
    const Nothofagus::BellotaId textureAnchorId =
        canvas.addBellota({{{0.0f, 0.0f}}, textureId});
    canvas.bellota(textureAnchorId).visual().visible() = false;

    struct Sprite
    {
        Nothofagus::BellotaId id;
        glm::vec2 home;
        float phase;
        float age;
        float lifespan;
    };
    std::vector<Sprite> sprites;
    Lcg rng{0x2468ace0u};

    // Interactive controls — owned + read/written only by the sim thread.
    int   targetCount   = 40;
    float scaleScale    = 1.0f;
    float waveSpeed     = 1.0f;
    bool  spawning      = true;
    float lastFps       = 0.0f;
    char  noteBuf[64]   = "type here (Ctrl+C/V works)";

    auto spawnOne = [&]()
    {
        const float x = rng.range(15.0f, screenSize.width - 15.0f);
        const float y = rng.range(15.0f, screenSize.height - 15.0f);
        const Nothofagus::BellotaId id = canvas.addBellota({{{x, y}, 1.0f}, textureId});
        sprites.push_back({id, glm::vec2(x, y), rng.range(0.0f, 6.28f), 0.0f, rng.range(1500.0f, 4000.0f)});
    };

    float simTime = 0.0f;

    // Game logic — runs lock-free on the sim thread (no ImGui here).
    auto update = [&](float dt)
    {
        simTime += dt;
        if (dt > 0.0f) lastFps = 0.9f * lastFps + 0.1f * (1000.0f / dt);

        // While the cursor is over the panel, freeze world activity (demonstrates
        // imguiWantsMouse(): UI focus suppresses the world). Crucially this gates
        // *aging/despawn together with spawning* — gating only spawning would let
        // the population drain to nothing while you interact with the controls.
        const bool worldActive = !canvas.imguiWantsMouse();

        // --- Simulation: animate + age/despawn ---
        for (std::size_t i = 0; i < sprites.size();)
        {
            Sprite& sprite = sprites[i];
            if (worldActive) sprite.age += dt;
            const bool expired  = worldActive && sprite.age >= sprite.lifespan;
            const bool overTarget = static_cast<int>(i) >= targetCount;
            if (expired || overTarget)
            {
                canvas.removeBellota(sprite.id);
                sprite = sprites.back();
                sprites.pop_back();
                continue;
            }
            Nothofagus::Bellota& bellota = canvas.bellota(sprite.id);
            const float wave = 0.0025f * simTime * waveSpeed + sprite.phase;
            bellota.transform().location().x = sprite.home.x + 8.0f * std::sin(wave);
            bellota.transform().location().y = sprite.home.y + 8.0f * std::cos(wave * 0.7f);
            const float life = sprite.age / sprite.lifespan;
            const float fade = std::sin(life * 3.14159f);
            bellota.transform().scale() = glm::vec2((0.4f + 1.2f * fade) * scaleScale);
            bellota.transform().angle() += 0.05f * dt;
            ++i;
        }

        // Refill toward the target — paused (with aging, above) while interacting.
        if (spawning && worldActive)
        {
            int budget = 3;
            while (static_cast<int>(sprites.size()) < targetCount && budget-- > 0)
                spawnOne();
        }
    };

    // Interactive ImGui panel — runs on the sim-UI context (sim thread), mouse
    // input marshalled from the render thread. Drives the controls the game logic
    // above reads.
    auto ui = [&](float /*dt*/)
    {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Once);
        ImGui::Begin("Simulation controls");
        ImGui::Text("sim %.0f fps   sprites %d", lastFps, (int)sprites.size());
        ImGui::SliderInt("target count", &targetCount, 0, 120);
        ImGui::SliderFloat("scale", &scaleScale, 0.3f, 3.0f);
        ImGui::SliderFloat("wave speed", &waveSpeed, 0.0f, 4.0f);
        ImGui::Checkbox("spawning", &spawning);
        if (ImGui::Button("clear all"))
            targetCount = 0;

        // Text input (exercises keyboard + the in-process clipboard).
        ImGui::InputText("note", noteBuf, sizeof(noteBuf));

        // Keyboard shortcut: Space toggles spawning — but not while typing into a
        // text field (so the space character goes to the InputText instead).
        if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space))
            spawning = !spawning;

        ImGui::TextDisabled("space: toggle spawning   capture m/k: %d/%d",
                            ImGui::GetIO().WantCaptureMouse ? 1 : 0,
                            ImGui::GetIO().WantCaptureKeyboard ? 1 : 0);
        ImGui::End();
    };

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        canvas.close();
    });

    canvas.beginThreadedSession(controller);

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
            canvas.commit(dt, update, ui);
            std::this_thread::sleep_for(targetPeriod);
        }
    });

    while (canvas.isThreadedRunning())
        canvas.renderFrame(controller);

    simThread.join();
    return 0;
}
