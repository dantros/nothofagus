// hello_threaded_dense_land — Dense/Sparse land explorers on the sim thread.
//
// The DenseLand world + DenseLandExplorer pool are driven from commit()'s update
// on the SIM thread: the camera is panned via denseLandExplorer.setCamera(...) and
// the world is edited via denseLand.setCell(...). nothofagus runs the explorer
// pre-pass (chunk re-sync: pool-texture setMapBulk + pool-bellota repositioning)
// inside produce(Threaded), serialized against the render thread by the asset
// mutex — so the pooled huge-world renderer works on the threaded driver with no
// app-side changes beyond owning the two loops.
//
// Camera: WASD (polled on the sim controller) or the auto-pan circular sweep,
// which forces a steady stream of border-slot chunk swaps every frame. Escape
// (render controller) closes.

#include <nothofagus.h>
#include <glm/glm.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

namespace
{
constexpr std::uint8_t kBlack = 1;

std::vector<std::uint8_t> makeBorderedTile(glm::ivec2 tileSize, std::uint8_t fillIndex)
{
    std::vector<std::uint8_t> data(static_cast<std::size_t>(tileSize.x * tileSize.y), fillIndex);
    for (int x = 0; x < tileSize.x; ++x)
    {
        data[static_cast<std::size_t>(x)]                                       = kBlack;
        data[static_cast<std::size_t>((tileSize.y - 1) * tileSize.x + x)]       = kBlack;
    }
    for (int y = 0; y < tileSize.y; ++y)
    {
        data[static_cast<std::size_t>(y * tileSize.x)]                          = kBlack;
        data[static_cast<std::size_t>(y * tileSize.x + (tileSize.x - 1))]       = kBlack;
    }
    return data;
}
}

int main()
{
    spdlog::info("hello_threaded_dense_land: explorers driven from the sim thread");

    constexpr glm::ivec2 tileSize {16, 16};
    constexpr glm::ivec2 chunkSize{16, 16};
    constexpr glm::ivec2 mapSize  {256, 256};
    constexpr int        pixelScale = 2;

    Nothofagus::Canvas canvas({480, 320}, "Hello Threaded DenseLand", {0.05f, 0.05f, 0.07f}, pixelScale);

    Nothofagus::ColorPallete palette{
        {0.0f, 0.0f, 0.0f, 0.0f},    // 0 transparent
        {0.0f, 0.0f, 0.0f, 1.0f},    // 1 black
        {1.0f, 1.0f, 1.0f, 1.0f},    // 2 white
        {0.85f, 0.20f, 0.20f, 1.0f}, // 3 red
        {0.95f, 0.85f, 0.20f, 1.0f}, // 4 yellow
        {0.20f, 0.45f, 0.85f, 1.0f}, // 5 blue
        {0.20f, 0.75f, 0.35f, 1.0f}, // 6 green
    };

    // Atlas: layers 0..3 are the four bordered band colors (palette 3..6).
    std::vector<std::vector<std::uint8_t>> tileGraphics;
    for (std::uint8_t color = 3; color <= 6; ++color)
        tileGraphics.push_back(makeBorderedTile(tileSize, color));

    Nothofagus::DenseLandId denseLandId = canvas.addDenseLand(
        Nothofagus::DenseLand(mapSize, chunkSize, tileSize, palette,
            std::span<const std::vector<std::uint8_t>>(tileGraphics)));
    Nothofagus::DenseLandExplorerId explorerId =
        canvas.addDenseLandExplorer(Nothofagus::DenseLandExplorer(denseLandId));

    // Banded pattern over the whole world (created up front, single-threaded).
    for (int row = 0; row < mapSize.y; ++row)
        for (int col = 0; col < mapSize.x; ++col)
            canvas.denseLand(denseLandId).setCell({col, row},
                static_cast<std::uint8_t>(((row / 8 + col / 8) % 4)));

    // ----- Sim controller: WASD camera, polled inside update on the sim thread.
    Nothofagus::Controller simController;

    // ----- Render controller: Escape closes (window input on the main thread).
    Nothofagus::Controller renderController;
    renderController.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.close(); });

    glm::vec2 camera{0.0f, 0.0f};
    float     autoPanPhase = 0.0f;
    bool      autoPan      = true; // headless-friendly stress: constant chunk swaps
    float     resizeTimerMs = 0.0f; // toggles the logical canvas size to exercise pool resize
    bool      bigCanvas     = true;
    std::uint32_t rngState = 0x9E3779B9u;
    auto nextRandomCell = [&]() -> glm::ivec2
    {
        rngState = rngState * 1664525u + 1013904223u;
        const int col = static_cast<int>((rngState >> 8) % static_cast<std::uint32_t>(mapSize.x));
        rngState = rngState * 1664525u + 1013904223u;
        const int row = static_cast<int>((rngState >> 8) % static_cast<std::uint32_t>(mapSize.y));
        return {col, row};
    };

    auto update = [&](float deltaTimeMS)
    {
        const float dt = deltaTimeMS / 1000.0f;
        constexpr float panSpeed = 200.0f; // world px/s

        glm::vec2 dir{0.0f, 0.0f};
        if (simController.isKeyDown(Nothofagus::Key::W)) dir.y += 1.0f;
        if (simController.isKeyDown(Nothofagus::Key::S)) dir.y -= 1.0f;
        if (simController.isKeyDown(Nothofagus::Key::A)) dir.x -= 1.0f;
        if (simController.isKeyDown(Nothofagus::Key::D)) dir.x += 1.0f;

        if (dir.x != 0.0f || dir.y != 0.0f)
        {
            autoPan = false;
            camera += glm::normalize(dir) * panSpeed * dt;
        }
        else if (autoPan)
        {
            autoPanPhase += dt * 0.25f * 2.0f * 3.14159265f;
            const glm::vec2 worldCenter{0.5f * mapSize.x * tileSize.x, 0.5f * mapSize.y * tileSize.y};
            camera = worldCenter + glm::vec2{1000.0f * std::cos(autoPanPhase), 1000.0f * std::sin(autoPanPhase)};
        }

        canvas.denseLandExplorer(explorerId).setCamera(camera);

        // A few random world edits per commit — bumps chunk generations so the pool
        // re-syncs displayed slots (exercises the threaded explorer write path).
        for (int i = 0; i < 8; ++i)
            canvas.denseLand(denseLandId).setCell(nextRandomCell(), 2);

        // Every ~2 s, change the logical canvas size from the SIM thread — exercises
        // A4: setScreenSize is now race-safe (atomic mScreenSize) and the explorer
        // pre-pass rebuilds the pool for the new size next commit.
        resizeTimerMs += deltaTimeMS;
        if (resizeTimerMs >= 2000.0f)
        {
            resizeTimerMs = 0.0f;
            bigCanvas = !bigCanvas;
            canvas.setScreenSize(bigCanvas ? Nothofagus::ScreenSize{480, 320}
                                           : Nothofagus::ScreenSize{360, 240});
        }
    };

    canvas.beginThreadedSession(renderController);

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

    while (canvas.isThreadedRunning())
        canvas.renderFrame(renderController);

    simThread.join();

    // Tear down explorers before their dense land (explorer-managed pool slots).
    canvas.removeDenseLandExplorer(explorerId);
    canvas.removeDenseLand(denseLandId);
    return 0;
}
