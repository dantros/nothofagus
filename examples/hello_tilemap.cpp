/// hello_tilemap.cpp
/// Demonstrates TileMapTexture: a memory-efficient tile map built from a
/// small atlas of RGBA tiles.  Three distinct tiles (grass, water, dirt) are
/// placed on a 20×15 grid.  Pressing SPACE cycles the center cell through the
/// three tile types, verifying dynamic map updates at runtime.

#include <nothofagus.h>
#include <cstdint>
#include <vector>
#include <array>

// Build one solid-colour RGBA tile (tileSize.x * tileSize.y * 4 bytes)
static std::vector<std::uint8_t> makeSolidTile(glm::ivec2 tileSize, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    const std::size_t numPixels = static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);
    std::vector<std::uint8_t> data(numPixels * 4);
    for (std::size_t i = 0; i < numPixels; ++i)
    {
        data[i * 4 + 0] = r;
        data[i * 4 + 1] = g;
        data[i * 4 + 2] = b;
        data[i * 4 + 3] = 255;
    }
    return data;
}

int main()
{
    constexpr glm::ivec2 tileSize{8, 8};      // pixels per tile
    constexpr glm::ivec2 mapSize{20, 15};     // cells (columns × rows)

    // Canvas: mapSize * tileSize pixels, 4× pixel scale → 640×480 window
    Nothofagus::Canvas canvas(
        {mapSize.x * tileSize.x, mapSize.y * tileSize.y},
        "Hello TileMap!",
        {0.1f, 0.1f, 0.1f},
        4
    );

    // --- Build a TileMapTexture ---
    Nothofagus::TileMapTexture tileMap(tileSize, mapSize);

    // Tile 0 — grass (green)
    auto grassPixels = makeSolidTile(tileSize, 34,  139, 34);
    tileMap.setTilePixels(0, grassPixels);

    // Tile 1 — water (blue)
    auto waterPixels = makeSolidTile(tileSize, 30,  144, 255);
    tileMap.setTilePixels(1, waterPixels);

    // Tile 2 — dirt (brown)
    auto dirtPixels  = makeSolidTile(tileSize, 139, 90,  43);
    tileMap.setTilePixels(2, dirtPixels);

    // Fill the map: alternating grass (0) with a band of water (1) and dirt (2)
    for (int row = 0; row < mapSize.y; ++row)
    {
        for (int col = 0; col < mapSize.x; ++col)
        {
            std::uint8_t tile = 0; // grass default
            if (row == mapSize.y / 2)  tile = 1; // water row through the middle
            if (col == mapSize.x / 2)  tile = 2; // dirt column through the middle
            tileMap.setCell(col, row, tile);
        }
    }

    Nothofagus::TextureId tileMapTexId = canvas.addTexture(tileMap);

    // Place a bellota centred on the canvas at scale 1:1.
    // The mesh is already sized to the texture dimensions (mapSize * tileSize),
    // so scale = 1 fills the canvas exactly.
    const glm::vec2 centre{
        static_cast<float>(mapSize.x * tileSize.x) * 0.5f,
        static_cast<float>(mapSize.y * tileSize.y) * 0.5f
    };
    Nothofagus::BellotaId spriteId = canvas.addBellota({
        Nothofagus::Transform(centre),
        tileMapTexId
    });

    // Track which tile type occupies the center cell
    std::uint8_t centerTile = 0;

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.close(); });

    controller.registerAction({Nothofagus::Key::SPACE, Nothofagus::DiscreteTrigger::Press},
        [&]()
        {
            centerTile = static_cast<std::uint8_t>((centerTile + 1) % 3);
            // Mutate the CPU-side TileMapTexture through the canvas reference
            auto& tm = std::get<Nothofagus::TileMapTexture>(canvas.texture(tileMapTexId));
            tm.setCell(mapSize.x / 2, mapSize.y / 2, centerTile);
            canvas.markTextureAsDirty(tileMapTexId);
        });

    canvas.run([](float /*dt*/) {}, controller);

    return 0;
}
