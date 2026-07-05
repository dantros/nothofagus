// hello_threaded_gamepad — game input (gamepad + keyboard + mouse) on the sim thread.
//
// The game logic runs on its own std::thread (inside canvas.commit's update);
// the main thread renders the previous frame's snapshot. nothofagus spawns no
// threads — this file owns both loops.
//
// The threaded path uses TWO controllers:
//   * a RENDER controller, bound to the window on the main thread (here: Escape
//     -> close). The window backend polls the OS input — keyboard/mouse/gamepad —
//     into it on the render thread.
//   * a SIM controller, the game's own. nothofagus marshals the render
//     controller's input — gamepad state (M5) plus held keyboard/mouse state and
//     per-frame scroll — across the thread boundary and replays it onto the sim
//     controller right before each commit's update, so the game can poll it
//     (getGamepadAxis / isKeyDown / isMouseButtonDown / getMousePosition) and
//     receive its callbacks (registerGamepadAction / registerAction / ...) the
//     normal way — all on the sim thread.
//
// Controls: left stick or WASD move the sprite; hold left mouse to ease it toward
// the cursor; gamepad Start or the R key toggle rotation; A pulses; D-pad steps.
// Input is one frame stale by construction (the uniform sim<-render latency).
// Real stick input needs a connected pad; with none, the marshal/feed runs every
// frame as a clean no-op.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>
#include <nothofagus.h>

int main()
{
    spdlog::info("hello_threaded_gamepad: gamepad input on the sim thread (two-controller model)");

    const Nothofagus::ScreenSize screenSize{200, 150};
    Nothofagus::Canvas canvas(screenSize, "Hello Threaded Gamepad", {0.15f, 0.15f, 0.2f}, 6);

    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},
        {0.2, 0.5, 0.9, 1.0},
        {0.4, 0.7, 1.0, 1.0},
        {0.6, 0.9, 1.0, 1.0},
    };

    Nothofagus::IndirectTexture texture({8, 8}, {0.5, 0.5, 0.5, 1.0});
    texture.setPallete(pallete)
        .setPixels(
        {
            0,0,3,3,3,3,0,0,
            0,3,2,2,2,2,3,0,
            3,2,1,2,2,1,2,3,
            3,2,2,2,2,2,2,3,
            3,2,2,2,2,2,2,3,
            3,2,1,2,2,1,2,3,
            0,3,2,2,2,2,3,0,
            0,0,3,3,3,3,0,0,
        });
    // Resources are created up front (uploaded once on the render thread) before
    // the threads start; at runtime the sim only mutates the bellota's values.
    const Nothofagus::TextureId textureId = canvas.addTexture(texture);
    const Nothofagus::BellotaId bellotaId = canvas.addBellota({{{100.0f, 75.0f}}, textureId});

    // Show the built-in FPS/ms overlay (render-thread frame time). On the threaded
    // path this draws on the main context, which is the rendered UI here since this
    // demo commits no sim ImGui.
    canvas.stats() = true;

    constexpr float horizontalSpeed = 0.12f;
    constexpr float angularSpeed    = 0.1f;
    constexpr float discreteStep    = 10.0f;
    bool  rotate     = false;
    float pulseTimer = 0.0f;

    // ----- Sim controller: the game's gamepad input, consumed on the sim thread.
    // Register all actions before the threads start (the sim thread is the only
    // one that touches this controller afterward).
    Nothofagus::Controller simController;

    simController.registerGamepadAction({0, Nothofagus::GamepadButton::A, Nothofagus::DiscreteTrigger::Press},
        [&]() { pulseTimer = 500.0f; });
    simController.registerGamepadAction({0, Nothofagus::GamepadButton::Start, Nothofagus::DiscreteTrigger::Press},
        [&]() { rotate = not rotate; });
    simController.registerGamepadAction({0, Nothofagus::GamepadButton::DpadUp, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().y += discreteStep; });
    simController.registerGamepadAction({0, Nothofagus::GamepadButton::DpadDown, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().y -= discreteStep; });
    simController.registerGamepadAction({0, Nothofagus::GamepadButton::DpadLeft, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().x -= discreteStep; });
    simController.registerGamepadAction({0, Nothofagus::GamepadButton::DpadRight, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().x += discreteStep; });

    // Keyboard (R) toggles rotation too — a registered key action on the sim
    // controller, dispatched on the sim thread (parity with the gamepad Start).
    simController.registerAction({Nothofagus::Key::R, Nothofagus::DiscreteTrigger::Press},
        [&]() { rotate = not rotate; });

    auto update = [&](float deltaTime)
    {
        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);

        // Smooth movement via the left stick (polling the sim controller).
        const std::vector<int> connectedIds = simController.getConnectedGamepadIds();
        if (not connectedIds.empty())
        {
            const int gamepadId = connectedIds[0];
            const float leftX = simController.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftX);
            const float leftY = simController.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftY);
            bellota.transform().location().x += leftX * horizontalSpeed * deltaTime;
            bellota.transform().location().y += leftY * horizontalSpeed * deltaTime;
        }

        // Keyboard WASD movement (polling the sim controller's held key state).
        glm::vec2 keyboardMove{0.0f, 0.0f};
        if (simController.isKeyDown(Nothofagus::Key::W)) keyboardMove.y += 1.0f;
        if (simController.isKeyDown(Nothofagus::Key::S)) keyboardMove.y -= 1.0f;
        if (simController.isKeyDown(Nothofagus::Key::D)) keyboardMove.x += 1.0f;
        if (simController.isKeyDown(Nothofagus::Key::A)) keyboardMove.x -= 1.0f;
        bellota.transform().location() += keyboardMove * horizontalSpeed * deltaTime;

        // Mouse: while the left button is held, ease the sprite toward the cursor
        // (mouse position arrives in canvas space, like the single-threaded path).
        if (simController.isMouseButtonDown(Nothofagus::MouseButton::Left))
        {
            const glm::vec2 cursor = simController.getMousePosition();
            bellota.transform().location() += (cursor - bellota.transform().location()) * 0.01f * deltaTime;
        }

        bellota.transform().location().x = std::clamp(bellota.transform().location().x, 10.0f, static_cast<float>(screenSize.width) - 10.0f);
        bellota.transform().location().y = std::clamp(bellota.transform().location().y, 10.0f, static_cast<float>(screenSize.height) - 10.0f);

        if (rotate)
            bellota.transform().angle() += angularSpeed * deltaTime;

        if (pulseTimer > 0.0f)
        {
            pulseTimer -= deltaTime;
            const float pulse = 1.0f + 0.5f * std::sin(pulseTimer * 0.02f);
            bellota.transform().scale() = glm::vec2(pulse * 3.0f, pulse * 3.0f);
        }
        else
        {
            bellota.transform().scale() = glm::vec2(3.0f, 3.0f);
        }
    };

    // ----- Render controller: window input on the main thread (Escape -> close).
    Nothofagus::Controller renderController;
    renderController.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.close(); });

    canvas.beginThreadedSession(renderController);

    // Simulation thread: commit at ~120 Hz, decoupled from the render cadence.
    // The gamepad-aware commit overload feeds simController before update.
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

            canvas.commit(dt, update, simController);

            std::this_thread::sleep_for(targetPeriod);
        }
    });

    // Main thread: render the latest committed snapshot + pump window/input.
    while (canvas.isThreadedRunning())
    {
        canvas.renderFrame(renderController);
    }

    simThread.join();
    return 0;
}
