/// hello_tilemap.cpp
/// Demonstrates TileMapTexture: a palette-indexed tile map.  The atlas stores
/// palette indices (1 byte per pixel); a single shared 256-colour palette
/// resolves the final RGBA per fragment.  Three distinct tiles (grass, water,
/// dirt) are placed on a 20×15 grid.  Pressing SPACE cycles the center cell
/// through the three tile types, verifying dynamic map updates at runtime.

#include <nothofagus.h>
#include <cstdint>
#include <vector>
#include <array>

// Build one solid palette-index tile (tileSize.x * tileSize.y bytes).
static std::vector<std::uint8_t> makeSolidTile(glm::ivec2 tileSize, std::uint8_t paletteIndex)
{
    const std::size_t numPixels = static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);
    return std::vector<std::uint8_t>(numPixels, paletteIndex);
}

int main()
{
    constexpr glm::ivec2 tileSize{8, 8};       // pixels per tile
    constexpr glm::ivec2 mapSize{20, 15};      // cells (columns × rows)

    // Canvas: mapSize * tileSize pixels, 8× pixel scale → 1280×960 window
    Nothofagus::Canvas canvas(
        {mapSize.x * tileSize.x, mapSize.y * tileSize.y},
        "Hello TileMap!",
        {0.1f, 0.1f, 0.1f},
        8
    );

    // --- Build a palette-indexed TileMapTexture ---
    // Palette index 0 — transparent placeholder (default).
    // Palette index 1 — grass (green), 2 — water (blue), 3 — dirt (brown).
    Nothofagus::TileMapTexture tileMap(tileSize, mapSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    tileMap.setPallete(Nothofagus::ColorPallete{
        {0.0f, 0.0f, 0.0f, 0.0f},                                  // 0 transparent
        {34.0f / 255.0f,  139.0f / 255.0f, 34.0f / 255.0f,  1.0f}, // 1 grass
        {30.0f / 255.0f,  144.0f / 255.0f, 255.0f / 255.0f, 1.0f}, // 2 water
        {139.0f / 255.0f, 90.0f / 255.0f,  43.0f / 255.0f,  1.0f}, // 3 dirt
    });

    // Tile 0 — grass (palette index 1)
    tileMap.setTilePixels(0, makeSolidTile(tileSize, 1));

    // Tile 1 — water (palette index 2)
    tileMap.setTilePixels(1, makeSolidTile(tileSize, 2));

    // Tile 2 — dirt (palette index 3)
    tileMap.setTilePixels(2, makeSolidTile(tileSize, 3));

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
