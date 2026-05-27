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
///
/// ImGui controls:
///   - Memory breakdown — watch the world cell grid scale while the pool stays flat
///   - Teleport         — jump the camera to an arbitrary world cell
///   - Recreate         — replace the tilemap with a different mapSize

#include <nothofagus.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
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
    const int tileWidth  = tileSize.x;
    const int tileHeight = tileSize.y;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(tileWidth * tileHeight), fillIndex);
    for (int x = 0; x < tileWidth; ++x)
    {
        data[static_cast<std::size_t>(x)]                                          = Pal::Black;
        data[static_cast<std::size_t>((tileHeight - 1) * tileWidth + x)]           = Pal::Black;
    }
    for (int y = 0; y < tileHeight; ++y)
    {
        data[static_cast<std::size_t>(y * tileWidth)]                              = Pal::Black;
        data[static_cast<std::size_t>(y * tileWidth + (tileWidth - 1))]            = Pal::Black;
    }
    return data;
}

// White-on-black tile with a single 8x8 ASCII glyph centered in the cell.
// Uses Nothofagus::writeChar to rasterise the glyph (font8x8 basic), then
// remaps writeChar's colorIds (0 = bg, 1 = fg) to palette indices
// (Pal::Black, Pal::White) so the digit is white-on-black in the final atlas.
static std::vector<std::uint8_t> makeDigitTile(glm::ivec2 tileSize,
                                                char asciiDigit,
                                                const Nothofagus::ColorPallete& palette)
{
    Nothofagus::IndirectTexture scratch(tileSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), 1);
    scratch.setPallete(palette); // shared palette so colorIds 0 and 1 validate
    Nothofagus::writeChar(scratch, static_cast<std::uint8_t>(asciiDigit), 4, 4);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(tileSize.x * tileSize.y), Pal::Black);
    for (int j = 0; j < tileSize.y; ++j)
    {
        for (int i = 0; i < tileSize.x; ++i)
        {
            if (scratch.pixel(static_cast<std::size_t>(i), static_cast<std::size_t>(j)).colorId == 1)
                data[static_cast<std::size_t>(j * tileSize.x + i)] = Pal::White;
        }
    }
    return data;
}

// Layer index for digit N (0..9) in the tileGraphics atlas built below.
// Layer layout: 0 = white solid, 1..4 = bordered colors, 5..14 = digits 0..9.
constexpr std::uint8_t digitLayer(int digit)
{
    return static_cast<std::uint8_t>(5 + digit);
}

// Pretty-print a byte count into a small buffer.
static void fmtBytes(char* out, std::size_t outSize, std::size_t bytes)
{
    if (bytes < 1024u)
        std::snprintf(out, outSize, "%zu B", bytes);
    else if (bytes < 1024u * 1024u)
        std::snprintf(out, outSize, "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(out, outSize, "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

int main()
{
    constexpr glm::ivec2 tileSize  {16, 16};
    constexpr glm::ivec2 chunkSize {16, 16};      // 16×16 cells per chunk = 256×256 px per slot
    constexpr glm::ivec2 initialMapSize{256, 256};
    constexpr int        pixelScale = 2;
    constexpr int        canvasWidth  = 480;
    constexpr int        canvasHeight = 320;

    Nothofagus::Canvas canvas(
        { canvasWidth, canvasHeight },             // canvas: 480×320 logical pixels
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

    // Tile atlas layout:
    //   0       white solid (legacy; no longer used by the populate loop)
    //   1..4    bordered colors used by the band pattern
    //   5..14   digits 0..9 (used by the chunk-label overlay)
    std::vector<std::vector<std::uint8_t>> tileGraphics{
        makeSolidTile   (tileSize, Pal::White),   // layer 0
        makeBorderedTile(tileSize, Pal::Red),     // layer 1
        makeBorderedTile(tileSize, Pal::Yellow),  // layer 2
        makeBorderedTile(tileSize, Pal::Blue),    // layer 3
        makeBorderedTile(tileSize, Pal::Green),   // layer 4
    };
    for (char d = '0'; d <= '9'; ++d)
        tileGraphics.push_back(makeDigitTile(tileSize, d, palette));

    // ── State that survives a recreate ─────────────────────────────────────
    glm::ivec2 mapSize = initialMapSize;
    glm::vec2  camera{0.0f, 0.0f};
    int        teleportCellX = 0;
    int        teleportCellY = 0;
    int        newCols = mapSize.x;
    int        newRows = mapSize.y;

    // ── Stress controls (exercise the chunk re-sync hot path) ─────────────
    bool       autoPan     = false;
    float      autoPanRadiusPx = 1000.0f;  // circular pan radius around world center
    float      autoPanRateHz   = 0.25f;    // revolutions per second
    float      autoPanPhase    = 0.0f;
    int        editsPerFrame   = 0;        // random setCell calls per frame
    std::uint32_t rngState     = 0x9E3779B9u;

    // ── Canvas resize controls (exercises M4 dynamic pool resize) ─────────
    int newCanvasWidth  = canvasWidth;
    int newCanvasHeight = canvasHeight;

    // Fills a tilemap with a banded pattern that cycles through layers 1..4,
    // then overlays chunk row/col index labels at each chunk's top-left:
    //   line 1 (top row of chunk): chunk row index digits
    //   line 2 (one cell below):   chunk col index digits
    // Labels are white-on-black, one digit per cell, left-aligned. With
    // chunkSize {16, 16} and 8x8 color bands, each chunk spans 2x2 mega-blocks.
    auto populateWorld = [&](Nothofagus::Tilemap& world, glm::ivec2 size)
    {
        // Base band pattern (the four bordered colors).
        for (int row = 0; row < size.y; ++row)
        {
            for (int col = 0; col < size.x; ++col)
            {
                const int band  = (row / 8) + (col / 8);
                const std::uint8_t layerIdx = static_cast<std::uint8_t>((band % 4) + 1);
                world.setCell({col, row}, layerIdx);
            }
        }
        // Chunk row/col label overlay (replaces the cells under the labels).
        const glm::ivec2 chunkGrid{
            (size.x + chunkSize.x - 1) / chunkSize.x,
            (size.y + chunkSize.y - 1) / chunkSize.y
        };
        auto writeChunkLabel = [&](int chunkRow, int chunkCol)
        {
            const int worldColOrigin = chunkCol * chunkSize.x;
            const int worldRowOrigin = chunkRow * chunkSize.y;

            auto stamp = [&](const std::string& digits, int worldRow)
            {
                for (std::size_t i = 0; i < digits.size(); ++i)
                {
                    const glm::ivec2 cellCoord{worldColOrigin + static_cast<int>(i), worldRow};
                    if (world.inBounds(cellCoord))
                        world.setCell(cellCoord, digitLayer(digits[i] - '0'));
                }
            };
            stamp(std::to_string(chunkRow), worldRowOrigin);     // top row of chunk
            stamp(std::to_string(chunkCol), worldRowOrigin + 1); // one cell below
        };
        for (int chunkRow = 0; chunkRow < chunkGrid.y; ++chunkRow)
            for (int chunkCol = 0; chunkCol < chunkGrid.x; ++chunkCol)
                writeChunkLabel(chunkRow, chunkCol);
    };

    // Build the initial Tilemap (world data) + TilemapView (pooled renderer).
    Nothofagus::TilemapHandles handles = Nothofagus::createTilemap(
        canvas, mapSize, chunkSize, tileSize, palette,
        std::span<const std::vector<std::uint8_t>>(tileGraphics));
    populateWorld(canvas.tilemap(handles.tilemapId), mapSize);

    // Tear down the current view+tilemap and rebuild at a new size. Safe to call
    // from inside the update callback: removeTilemapView/removeTilemap drop pool
    // bellotas+textures and the world data; createTilemap registers fresh ones;
    // the per-frame view pass picks them up the same frame.
    auto rebuild = [&](glm::ivec2 newSize)
    {
        canvas.removeTilemapView(handles.viewId);
        canvas.removeTilemap(handles.tilemapId);
        mapSize = newSize;
        handles = Nothofagus::createTilemap(
            canvas, mapSize, chunkSize, tileSize, palette,
            std::span<const std::vector<std::uint8_t>>(tileGraphics));
        populateWorld(canvas.tilemap(handles.tilemapId), mapSize);
        camera = {0.0f, 0.0f};
    };

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
        // ── Camera pan ──────────────────────────────────────────────────
        const float dt = deltaTimeMS / 1000.0f;
        if (autoPan)
        {
            // Circular sweep around the world center — forces a steady stream
            // of border-slot chunk swaps every frame (exercises chunkDataInto).
            autoPanPhase += dt * autoPanRateHz * 2.0f * 3.14159265f;
            const glm::vec2 worldCenterPx{
                0.5f * static_cast<float>(mapSize.x * tileSize.x),
                0.5f * static_cast<float>(mapSize.y * tileSize.y)
            };
            camera = worldCenterPx + glm::vec2{
                autoPanRadiusPx * std::cos(autoPanPhase),
                autoPanRadiusPx * std::sin(autoPanPhase)
            };
        }
        else
        {
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
        }
        canvas.tilemapView(handles.viewId).setCamera(camera);

        // ── Edit storm ─────────────────────────────────────────────────
        // Randomly setCell across the world — each edit bumps its chunk's
        // generation counter, forcing chunkDataInto on the next updateViews
        // pass for whichever slot is painting that chunk.
        if (editsPerFrame > 0)
        {
            Nothofagus::Tilemap& world = canvas.tilemap(handles.tilemapId);
            auto next = [&] { rngState = rngState * 1664525u + 1013904223u; return rngState; };
            for (int i = 0; i < editsPerFrame; ++i)
            {
                const glm::ivec2 cellCoord{
                    static_cast<int>(next() % static_cast<std::uint32_t>(mapSize.x)),
                    static_cast<int>(next() % static_cast<std::uint32_t>(mapSize.y))
                };
                if (world.inBounds(cellCoord))
                    world.setCell(cellCoord, static_cast<std::uint8_t>(1 + (next() % 4)));
            }
        }

        // ── ImGui control panel ─────────────────────────────────────────
        ImGui::Begin("Tilemap");

        // Status
        const Nothofagus::ScreenSize liveCanvasSize = canvas.screenSize();
        ImGui::Text("WASD to pan, ESC to quit");
        ImGui::Text("camera = (%.1f, %.1f) px", camera.x, camera.y);
        ImGui::Text("canvas = %u x %u px",     liveCanvasSize.width, liveCanvasSize.height);
        ImGui::Text("world  = %d x %d cells",  mapSize.x, mapSize.y);
        ImGui::Text("chunk  = %d x %d cells (%d x %d px)",
                    chunkSize.x, chunkSize.y,
                    chunkSize.x * tileSize.x, chunkSize.y * tileSize.y);

        ImGui::Separator();

        // Memory breakdown (deterministic CPU byte counts for the new structures).
        {
            const std::size_t layerCount  = tileGraphics.size();
            const std::size_t paletteSize = palette.colors.size();

            // Tilemap world data
            const std::size_t cellGridBytes    = static_cast<std::size_t>(mapSize.x) * static_cast<std::size_t>(mapSize.y);
            const glm::ivec2  chunkGridSize    {
                (mapSize.x + chunkSize.x - 1) / chunkSize.x,
                (mapSize.y + chunkSize.y - 1) / chunkSize.y
            };
            const std::size_t chunkGensBytes   = static_cast<std::size_t>(chunkGridSize.x) * static_cast<std::size_t>(chunkGridSize.y) * sizeof(std::uint64_t);
            const std::size_t worldAtlasBytes  = layerCount * static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);
            const std::size_t worldPaletteBytes = paletteSize * sizeof(glm::vec4);
            const std::size_t worldTotalBytes  = cellGridBytes + chunkGensBytes + worldAtlasBytes + worldPaletteBytes;

            // Pool (matches the formula in TilemapManager::buildPoolSlots).
            // Reads canvas.screenSize() so the readout tracks runtime setScreenSize.
            const Nothofagus::ScreenSize liveScreen = canvas.screenSize();
            const glm::ivec2 chunkPixelSize{ chunkSize.x * tileSize.x, chunkSize.y * tileSize.y };
            const glm::ivec2 poolGridSize{
                (static_cast<int>(liveScreen.width)  + chunkPixelSize.x - 1) / chunkPixelSize.x + 2,
                (static_cast<int>(liveScreen.height) + chunkPixelSize.y - 1) / chunkPixelSize.y + 2
            };
            const std::size_t slotCount       = static_cast<std::size_t>(poolGridSize.x) * static_cast<std::size_t>(poolGridSize.y);
            const std::size_t slotAtlasBytes  = layerCount * static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);
            const std::size_t slotMapBytes    = static_cast<std::size_t>(chunkSize.x) * static_cast<std::size_t>(chunkSize.y);
            const std::size_t slotPaletteBytes = paletteSize * sizeof(glm::vec4);
            const std::size_t perSlotBytes    = slotAtlasBytes + slotMapBytes + slotPaletteBytes;
            const std::size_t poolTotalBytes  = slotCount * perSlotBytes;

            const std::size_t grandTotalBytes = worldTotalBytes + poolTotalBytes;

            char buf[64];
            ImGui::Text("Tilemap (world data):");
            fmtBytes(buf, sizeof(buf), cellGridBytes);     ImGui::Text("  cell grid:   %s", buf);
            fmtBytes(buf, sizeof(buf), chunkGensBytes);    ImGui::Text("  chunk gens:  %s", buf);
            fmtBytes(buf, sizeof(buf), worldAtlasBytes);   ImGui::Text("  atlas:       %s", buf);
            fmtBytes(buf, sizeof(buf), worldPaletteBytes); ImGui::Text("  palette:     %s", buf);
            fmtBytes(buf, sizeof(buf), worldTotalBytes);   ImGui::Text("  total:       %s", buf);

            ImGui::Text("Pool (%zu slots = %d x %d):",
                        slotCount, poolGridSize.x, poolGridSize.y);
            fmtBytes(buf, sizeof(buf), slotAtlasBytes);    ImGui::Text("  per-slot atlas:   %s", buf);
            fmtBytes(buf, sizeof(buf), slotMapBytes);      ImGui::Text("  per-slot map:     %s", buf);
            fmtBytes(buf, sizeof(buf), slotPaletteBytes);  ImGui::Text("  per-slot palette: %s", buf);
            fmtBytes(buf, sizeof(buf), poolTotalBytes);    ImGui::Text("  total:            %s", buf);

            fmtBytes(buf, sizeof(buf), grandTotalBytes);
            ImGui::Text("GRAND TOTAL: %s", buf);
        }

        ImGui::Separator();

        // Teleport — jump the camera to an arbitrary world cell.
        ImGui::Text("Teleport to cell:");
        ImGui::InputInt("cell X", &teleportCellX);
        ImGui::InputInt("cell Y", &teleportCellY);
        if (ImGui::Button("Go"))
        {
            teleportCellX = std::clamp(teleportCellX, 0, mapSize.x - 1);
            teleportCellY = std::clamp(teleportCellY, 0, mapSize.y - 1);
            // Camera takes a world-pixel coordinate; +0.5 centers the cell in view.
            camera = glm::vec2{
                (static_cast<float>(teleportCellX) + 0.5f) * static_cast<float>(tileSize.x),
                (static_cast<float>(teleportCellY) + 0.5f) * static_cast<float>(tileSize.y)
            };
        }

        ImGui::Separator();

        // Recreate — replace the tilemap with a fresh one at a new size.
        ImGui::Text("Recreate tilemap:");
        ImGui::InputInt("cols", &newCols);
        ImGui::InputInt("rows", &newRows);
        if (ImGui::Button("Recreate"))
        {
            newCols = std::max(1, newCols);
            newRows = std::max(1, newRows);
            rebuild({newCols, newRows});
            teleportCellX = std::min(teleportCellX, mapSize.x - 1);
            teleportCellY = std::min(teleportCellY, mapSize.y - 1);
        }

        ImGui::Separator();

        // Stress mode — hammers the chunk re-sync hot path (m1: chunkDataInto)
        // and exercises inBounds (m3). Toggle the stats overlay (canvas.stats())
        // to read frame time while these are on.
        ImGui::Text("Stress (perf):");
        ImGui::Checkbox("auto-pan (continuous chunk swaps)", &autoPan);
        if (autoPan)
        {
            ImGui::SliderFloat("radius (px)", &autoPanRadiusPx, 0.0f, 4000.0f);
            ImGui::SliderFloat("rate (Hz)",   &autoPanRateHz,   0.0f, 4.0f);
        }
        ImGui::SliderInt("edits/frame (gen bumps)", &editsPerFrame, 0, 5000);
        ImGui::Checkbox("show frame stats", &canvas.stats());

        ImGui::Separator();

        // Resize canvas — exercises M4 (TilemapView pool re-allocates against the new size).
        ImGui::Text("Resize canvas:");
        ImGui::InputInt("canvas w", &newCanvasWidth);
        ImGui::InputInt("canvas h", &newCanvasHeight);
        if (ImGui::Button("Resize"))
        {
            newCanvasWidth  = std::max(64, newCanvasWidth);
            newCanvasHeight = std::max(64, newCanvasHeight);
            canvas.setScreenSize({
                static_cast<unsigned int>(newCanvasWidth),
                static_cast<unsigned int>(newCanvasHeight)
            });
        }

        ImGui::End();
    }, controller);

    return 0;
}
