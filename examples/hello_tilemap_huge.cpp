/// hello_tilemap_huge.cpp
/// Demonstrates the pooled `Tilemap` + `TilemapView` huge-tilemap pipeline.
/// Builds a 256×256-cell world (≈64 chunks of 32×32) with a small tile atlas,
/// then lets the user pan with WASD via `tilemapView.setCamera(...)`. Only the
/// pool slots covering the visible window + a 1-chunk margin are drawn each
/// frame; world chunks rotate through the pool as the camera moves.
///
/// The world cell grid lives once inside the `Tilemap` (~64 KB for 256×256).
/// The pool allocates a handful of `IndirectTexture` + `Bellota` slots sized to
/// the canvas — regardless of how big the world is.

#include <nothofagus.h>
#include <imgui.h>
#include <cstdint>
#include <cmath>
#include <vector>

namespace Pal
{
    constexpr std::uint8_t Transparent = 0;
    constexpr std::uint8_t Black       = 1;
    constexpr std::uint8_t White       = 2;
    constexpr std::uint8_t Red         = 3;
    constexpr std::uint8_t Yellow      = 4;
    constexpr std::uint8_t Blue        = 5;
    constexpr std::uint8_t Green       = 6;
}

// Solid-color tile filled with one palette index.
static std::vector<std::uint8_t> makeSolidTile(glm::ivec2 tileSize, std::uint8_t paletteIndex)
{
    return std::vector<std::uint8_t>(static_cast<std::size_t>(tileSize.x * tileSize.y), paletteIndex);
}

// Tile with a black 1-pixel border around a solid color.
static std::vector<std::uint8_t> makeBorderedTile(glm::ivec2 tileSize, std::uint8_t fillIndex)
{
    const int w = tileSize.x, h = tileSize.y;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(w * h), fillIndex);
    for (int x = 0; x < w; ++x)
    {
        data[static_cast<std::size_t>(x)]                            = Pal::Black;
        data[static_cast<std::size_t>((h - 1) * w + x)]              = Pal::Black;
    }
    for (int y = 0; y < h; ++y)
    {
        data[static_cast<std::size_t>(y * w)]                        = Pal::Black;
        data[static_cast<std::size_t>(y * w + (w - 1))]              = Pal::Black;
    }
    return data;
}

int main()
{
    constexpr glm::ivec2 tileSize  {16, 16};
    constexpr glm::ivec2 chunkSize {16, 16};      // 16×16 cells per chunk = 256×256 px per slot
    constexpr glm::ivec2 mapSize   {256, 256};    // 256×256 world cells = ~4096×4096 world pixels
    constexpr int        pixelScale = 2;          // 2× zoom

    Nothofagus::Canvas canvas(
        { 480, 320 },                              // canvas: 480×320 logical pixels
        "Hello Huge Tilemap",
        { 0.05f, 0.05f, 0.07f },
        pixelScale
    );

    Nothofagus::ColorPallete palette{
        {0.0f, 0.0f, 0.0f, 0.0f},   // 0 transparent
        {0.0f, 0.0f, 0.0f, 1.0f},   // 1 black
        {1.0f, 1.0f, 1.0f, 1.0f},   // 2 white
        {0.85f, 0.20f, 0.20f, 1.0f},// 3 red
        {0.95f, 0.85f, 0.20f, 1.0f},// 4 yellow
        {0.20f, 0.45f, 0.85f, 1.0f},// 5 blue
        {0.20f, 0.75f, 0.35f, 1.0f},// 6 green
    };

    // Six tile graphics: a few solid colors and a few bordered variants.
    std::vector<std::vector<std::uint8_t>> tileGraphics{
        makeSolidTile   (tileSize, Pal::White),
        makeBorderedTile(tileSize, Pal::Red),
        makeBorderedTile(tileSize, Pal::Yellow),
        makeBorderedTile(tileSize, Pal::Blue),
        makeBorderedTile(tileSize, Pal::Green),
        makeSolidTile   (tileSize, Pal::Black),
    };

    // Build the Tilemap (world data) + TilemapView (pooled renderer) in one shot.
    auto handles = Nothofagus::createTilemap(
        canvas, mapSize, chunkSize, tileSize, palette,
        std::span<const std::vector<std::uint8_t>>(tileGraphics));

    // Populate the world cell grid with a pattern that varies slowly so the
    // boundary between chunks is visible as the camera scrolls.
    Nothofagus::Tilemap& world = canvas.tilemap(handles.tilemapId);
    for (int row = 0; row < mapSize.y; ++row)
    {
        for (int col = 0; col < mapSize.x; ++col)
        {
            const int band  = (row / 8) + (col / 8);
            const std::uint8_t layerIdx = static_cast<std::uint8_t>(
                (band % static_cast<int>(tileGraphics.size() - 1)) + 1);
            world.setCell({col, row}, layerIdx);
        }
    }

    // Place a single solid-white border tile at the world origin so the user can
    // find the camera-zero anchor while scrolling around.
    world.setCell({0, 0}, 0);

    glm::vec2 camera{0.0f, 0.0f};
    constexpr float panSpeed = 200.0f;   // world pixels per second

    // Track WASD as held state via Press/Release actions (Controller is event-driven).
    bool wDown = false, sDown = false, aDown = false, dDown = false;

    Nothofagus::Controller controller;
    controller.registerAction(
        { Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press },
        [&]() { canvas.close(); });
    controller.registerAction({ Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press   }, [&]() { wDown = true;  });
    controller.registerAction({ Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Release }, [&]() { wDown = false; });
    controller.registerAction({ Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press   }, [&]() { sDown = true;  });
    controller.registerAction({ Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Release }, [&]() { sDown = false; });
    controller.registerAction({ Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Press   }, [&]() { aDown = true;  });
    controller.registerAction({ Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Release }, [&]() { aDown = false; });
    controller.registerAction({ Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Press   }, [&]() { dDown = true;  });
    controller.registerAction({ Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Release }, [&]() { dDown = false; });

    canvas.run([&](float deltaTimeMS)
    {
        const float dt = deltaTimeMS / 1000.0f;
        glm::vec2 dir{0.0f, 0.0f};
        if (wDown) dir.y += 1.0f;
        if (sDown) dir.y -= 1.0f;
        if (aDown) dir.x -= 1.0f;
        if (dDown) dir.x += 1.0f;
        if (dir.x != 0.0f || dir.y != 0.0f)
        {
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            camera += (dir / len) * (panSpeed * dt);
        }
        canvas.tilemapView(handles.viewId).setCamera(camera);

        ImGui::Begin("Tilemap");
        ImGui::Text("WASD to pan, ESC to quit");
        ImGui::Text("camera = (%.1f, %.1f) px", camera.x, camera.y);
        ImGui::Text("world  = %d x %d cells",  mapSize.x, mapSize.y);
        ImGui::Text("chunk  = %d x %d cells (%d x %d px)",
                    chunkSize.x, chunkSize.y,
                    chunkSize.x * tileSize.x, chunkSize.y * tileSize.y);
        ImGui::End();
    }, controller);

    return 0;
}
