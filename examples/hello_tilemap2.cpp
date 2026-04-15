/// hello_tilemap2.cpp
/// Demonstrates TileMapTexture with two handcrafted 8×8 tiles arranged in a
/// 3×4 checkerboard grid:
///   Tile 0 — white circle on a black background
///   Tile 1 — blue→cyan diagonal ordered-dithered gradient (2×2 Bayer matrix)

#include <nothofagus.h>
#include <cstdint>
#include <vector>
#include <cmath>

// ---------------------------------------------------------------------------
// Tile 0: white filled circle on black background
// ---------------------------------------------------------------------------
static std::vector<std::uint8_t> makeCircleTile(glm::ivec2 tileSize)
{
    const int w = tileSize.x, h = tileSize.y;
    const float cx = (w - 1) * 0.5f;
    const float cy = (h - 1) * 0.5f;
    const float r2 = (w * 0.5f - 0.5f) * (w * 0.5f - 0.5f);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(w * h * 4));
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float dx = x - cx, dy = y - cy;
            const std::uint8_t v = (dx * dx + dy * dy <= r2) ? 255 : 0;
            const std::size_t i = static_cast<std::size_t>((y * w + x) * 4);
            data[i + 0] = v;
            data[i + 1] = v;
            data[i + 2] = v;
            data[i + 3] = 255;
        }
    }
    return data;
}

// ---------------------------------------------------------------------------
// Tile 1: blue→cyan diagonal gradient dithered with a 2×2 Bayer matrix.
//         Blue  = (0,   0, 255)   top-left corner
//         Cyan  = (0, 255, 255)   bottom-right corner
//         Only the green channel differs; the Bayer threshold decides per pixel.
// ---------------------------------------------------------------------------
static std::vector<std::uint8_t> makeDitherGradientTile(glm::ivec2 tileSize)
{
    const int w = tileSize.x, h = tileSize.y;
    const float maxD = static_cast<float>((w - 1) + (h - 1));

    // 2×2 Bayer ordered-dither thresholds
    static constexpr float kBayer[2][2] = { { 0.25f, 0.75f }, { 0.75f, 0.25f } };

    std::vector<std::uint8_t> data(static_cast<std::size_t>(w * h * 4));
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float d         = static_cast<float>(x + y) / maxD;
            const float threshold = kBayer[y % 2][x % 2];
            const bool  isCyan    = (d > threshold);

            const std::size_t i = static_cast<std::size_t>((y * w + x) * 4);
            data[i + 0] = 0;                   // R: always 0
            data[i + 1] = isCyan ? 255 : 0;    // G: 255 for cyan, 0 for blue
            data[i + 2] = 255;                  // B: always 255
            data[i + 3] = 255;                  // A: opaque
        }
    }
    return data;
}

// ---------------------------------------------------------------------------
int main()
{
    constexpr glm::ivec2 tileSize {8, 8};
    constexpr glm::ivec2 mapSize  {3, 4};   // 3 columns × 4 rows
    constexpr int        pixelScale = 8;    // 8× zoom → 192×256 window

    Nothofagus::Canvas canvas(
        { mapSize.x * tileSize.x, mapSize.y * tileSize.y },
        "Hello TileMap 2",
        { 0.0f, 0.0f, 0.0f },
        pixelScale
    );

    // Build the tile map
    Nothofagus::TileMapTexture tileMap(tileSize, mapSize);

    auto circlePx = makeCircleTile(tileSize);
    tileMap.setTilePixels(0, std::span<const std::uint8_t>(circlePx));

    auto ditherPx = makeDitherGradientTile(tileSize);
    tileMap.setTilePixels(1, std::span<const std::uint8_t>(ditherPx));

    // Checkerboard: (col + row) % 2 selects tile 0 or 1
    for (int row = 0; row < mapSize.y; ++row)
        for (int col = 0; col < mapSize.x; ++col)
            tileMap.setCell(col, row, static_cast<std::uint8_t>((col + row) % 2));

    Nothofagus::TextureId tileMapTexId = canvas.addTexture(tileMap);

    // One bellota centred on the canvas at scale 1:1.
    // The mesh is already sized to the texture dimensions (mapSize * tileSize),
    // so scale = 1 fills the canvas exactly.
    const glm::vec2 centre{
        static_cast<float>(mapSize.x * tileSize.x) * 0.5f,
        static_cast<float>(mapSize.y * tileSize.y) * 0.5f
    };
    canvas.addBellota({
        Nothofagus::Transform(centre),
        tileMapTexId
    });

    Nothofagus::Controller controller;
    controller.registerAction(
        { Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press },
        [&]() { canvas.close(); });

    canvas.run([](float) {}, controller);
    return 0;
}
