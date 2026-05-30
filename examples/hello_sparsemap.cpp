/// hello_sparsemap.cpp
/// Demonstrates the pooled `Sparsemap` + `SparsemapExplorer` sparse-tilemap pipeline —
/// the same chunk-pool optimization as `Tilemap`, but the world data lives in a
/// hash-map of chunks instead of a dense grid. There is no `mapSize`; the world is
/// unbounded and chunks exist only where added.
///
/// What this demo shows:
///   - An empty Sparsemap renders nothing (pool slots are hidden via `chunkInBounds`).
///   - **Visibly sparse world**: a deterministic per-coord hash decides whether each
///     chunk exists. With density at 25%, ~3 of every 4 chunk slots are empty — you
///     see colored "island" chunks scattered across mostly-empty space, with the
///     gaps rendered by the slot-hide-on-missing-chunk path.
///   - Streaming around the camera: as you pan, chunks that should exist (per the
///     hash) get `addChunk`'d on entry; chunks outside the unload radius get
///     `removeChunk`'d. Memory scales with `chunkCount`, not with how far you've
///     travelled. The same hash is deterministic, so revisiting a region restores
///     the same layout.
///   - Manual `addChunk` / `removeChunk` at specific coords from the UI — bypasses
///     the density gate, useful for placing or wiping individual chunks.
///   - `setCell` lazy-creates the owning chunk if absent — paint a single cell into
///     empty space and watch a fresh chunk appear next frame.

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

// Tile with a black 1-pixel border around a solid color (matches hello_tilemap_huge).
static std::vector<std::uint8_t> makeBorderedTile(glm::ivec2 tileSize, std::uint8_t fillIndex)
{
    const int tileWidth  = tileSize.x;
    const int tileHeight = tileSize.y;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(tileWidth * tileHeight), fillIndex);
    for (int x = 0; x < tileWidth; ++x)
    {
        data[static_cast<std::size_t>(x)]                                    = Pal::Black;
        data[static_cast<std::size_t>((tileHeight - 1) * tileWidth + x)]     = Pal::Black;
    }
    for (int y = 0; y < tileHeight; ++y)
    {
        data[static_cast<std::size_t>(y * tileWidth)]                        = Pal::Black;
        data[static_cast<std::size_t>(y * tileWidth + (tileWidth - 1))]      = Pal::Black;
    }
    return data;
}

// White-on-black tile with a single 8x8 ASCII glyph centered in the cell.
// Mirrors `makeDigitTile` from hello_tilemap_huge.cpp: rasterises the glyph with
// `Nothofagus::writeChar` (font8x8 basic, colorIds 0=bg / 1=fg) then remaps to
// palette indices so the glyph reads as white-on-black against the chunk color.
static std::vector<std::uint8_t> makeAsciiTile(glm::ivec2 tileSize,
                                                char asciiChar,
                                                const Nothofagus::ColorPallete& palette)
{
    Nothofagus::IndirectTexture scratch(tileSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), 1);
    scratch.setPallete(palette);
    Nothofagus::writeChar(scratch, static_cast<std::uint8_t>(asciiChar), 4, 4);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(tileSize.x * tileSize.y), Pal::Black);
    for (int j = 0; j < tileSize.y; ++j)
        for (int i = 0; i < tileSize.x; ++i)
            if (scratch.pixel(static_cast<std::size_t>(i), static_cast<std::size_t>(j)).colorId == 1)
                data[static_cast<std::size_t>(j * tileSize.x + i)] = Pal::White;
    return data;
}

// Atlas layer layout (matches the tileGraphics vector built in main()):
//   0..4    bordered color tiles (chunk background colors)
//   5..14   digit glyphs '0'..'9'
//   15      minus-sign glyph '-'  (sparsemap coords go negative)
constexpr std::uint8_t digitLayer(int digit) { return static_cast<std::uint8_t>(5 + digit); }
constexpr std::uint8_t minusLayer()           { return 15; }

// Color layer index (1..4) for a chunk, cycling deterministically by chunk coord.
static std::uint8_t patternForChunk(glm::ivec2 chunkPos)
{
    const int sum = std::abs(chunkPos.x) + std::abs(chunkPos.y);
    return static_cast<std::uint8_t>(1 + (sum % 4));
}

// Build the cell data for one chunk: fill with the chunk's color, then stamp the
// chunk's coordinate as a two-line label at the top-left — x on row 0, y on row 1.
// Each character occupies one cell, left-aligned; minus signs and digits map to
// their atlas layers via `minusLayer()` / `digitLayer(N)`.
static std::vector<std::uint8_t> buildChunkData(glm::ivec2 chunkSize, glm::ivec2 chunkPos)
{
    std::vector<std::uint8_t> data(
        static_cast<std::size_t>(chunkSize.x * chunkSize.y),
        patternForChunk(chunkPos));

    auto stampInt = [&](int value, int row)
    {
        if (row < 0 || row >= chunkSize.y) return;
        const std::string s = std::to_string(value);
        const int rowOffset = row * chunkSize.x;
        for (std::size_t i = 0; i < s.size() && static_cast<int>(i) < chunkSize.x; ++i)
        {
            const char c = s[i];
            const std::uint8_t layer = (c == '-') ? minusLayer()
                                                  : digitLayer(c - '0');
            data[static_cast<std::size_t>(rowOffset + static_cast<int>(i))] = layer;
        }
    };
    stampInt(chunkPos.x, 0);  // top-left: x coord
    stampInt(chunkPos.y, 1);  // one row below: y coord
    return data;
}

// Deterministic per-coord hash → "does a chunk exist here?" decision. Drives the
// streaming loop so the world looks like scattered islands rather than a solid grid.
// Same input always yields the same answer, so revisiting a region restores the same
// chunk layout. `densityPercent` in [0, 100] sets the rough fraction of populated chunks.
//
// Intentionally NOT `IVec2Hash`: we want uniform `% 100` distribution for the density
// gate, not the avalanche properties IVec2Hash provides for hash-map bucketing. Don't
// "simplify" this by routing it through IVec2Hash — they're solving different problems.
static bool chunkExistsAt(glm::ivec2 cp, int densityPercent)
{
    std::uint32_t h = static_cast<std::uint32_t>(cp.x) * 0x9e3779b9u
                    + static_cast<std::uint32_t>(cp.y) * 0x85ebca6bu;
    h ^= h >> 16; h *= 0x85ebca6bu;
    h ^= h >> 13; h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return static_cast<int>(h % 100u) < densityPercent;
}

static void formatBytes(char* out, std::size_t outSize, std::size_t bytes)
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
    constexpr glm::ivec2 tileSize    {16, 16};
    constexpr glm::ivec2 chunkSize   {16, 16};   // 16×16 cells per chunk = 256×256 px per slot
    constexpr int        pixelScale  = 2;
    constexpr int        canvasWidth = 480;
    constexpr int        canvasHeight= 320;

    Nothofagus::Canvas canvas(
        { canvasWidth, canvasHeight },
        "Hello Sparsemap",
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

    // Layout: 5 color tiles (layers 0..4), then digit glyphs 0..9 (5..14), then minus (15).
    // The buildChunkData helper picks color tiles via patternForChunk(...) and
    // overlays the label cells with digitLayer(N) / minusLayer().
    std::vector<std::vector<std::uint8_t>> tileGraphics{
        makeBorderedTile(tileSize, Pal::White),   // layer 0
        makeBorderedTile(tileSize, Pal::Red),     // layer 1
        makeBorderedTile(tileSize, Pal::Yellow),  // layer 2
        makeBorderedTile(tileSize, Pal::Blue),    // layer 3
        makeBorderedTile(tileSize, Pal::Green),   // layer 4
    };
    for (char d = '0'; d <= '9'; ++d)
        tileGraphics.push_back(makeAsciiTile(tileSize, d, palette));   // layers 5..14
    tileGraphics.push_back(makeAsciiTile(tileSize, '-', palette));     // layer 15

    // Register the Sparsemap (world data) and a SparsemapExplorer (pooled renderer)
    // against the canvas. The explorer takes the SparsemapId it draws from. Nothing
    // renders until we start populating chunks below.
    Nothofagus::SparsemapId sparsemapId = canvas.addSparsemap(
        Nothofagus::Sparsemap(chunkSize, tileSize, palette,
            std::span<const std::vector<std::uint8_t>>(tileGraphics)));
    Nothofagus::SparsemapExplorerId explorerId =
        canvas.addSparsemapExplorer(Nothofagus::SparsemapExplorer(sparsemapId));

    // No explicit initial seed — the first streaming pass below populates the visible
    // region using the deterministic density hash. With streaming off, the world starts
    // empty (nothing renders, all pool slots stay hidden via chunkInBounds).

    // ── UI state ─────────────────────────────────────────────────────────
    glm::vec2 camera{0.0f, 0.0f};
    bool      streamingOn      = true;
    int       sparsityDensityPercent = 25;  // ~25% of chunks in range actually exist
    int       loadRadiusChunks = 6;          // chunks inside this radius (Chebyshev) get evaluated
    int       unloadRadiusChunks = 10;       // chunks outside this radius get evicted
    int       manualAddX = 5,  manualAddY = 0;
    int       manualRemoveX = -3, manualRemoveY = 0;
    int       editWorldX = 0, editWorldY = 24, editLayer = 4;  // setCell editor

    constexpr float panSpeed = 1000.0f;   // world px / s — fast enough to cross ~4 chunks/s at 256-px chunks
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

        // ── Camera pan ──────────────────────────────────────────────────
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
        canvas.sparsemapExplorer(explorerId).setCamera(camera);

        Nothofagus::Sparsemap& world = canvas.sparsemap(sparsemapId);

        // ── Streaming around the camera (density-gated) ─────────────────
        // For each chunk slot inside the load radius, the deterministic density hash
        // decides whether it should exist. We sync the resident set to match: missing
        // chunks that should exist get added; resident chunks that no longer should
        // (e.g. after the user lowered the density slider) get removed. Chunks outside
        // the unload radius get evicted regardless — that's the streaming budget.
        if (streamingOn)
        {
            const glm::vec2 chunkPx{
                static_cast<float>(chunkSize.x * tileSize.x),
                static_cast<float>(chunkSize.y * tileSize.y)
            };
            const glm::ivec2 cameraChunk{
                static_cast<int>(std::floor(camera.x / chunkPx.x)),
                static_cast<int>(std::floor(camera.y / chunkPx.y))
            };
            for (int dy = -loadRadiusChunks; dy <= loadRadiusChunks; ++dy)
                for (int dx = -loadRadiusChunks; dx <= loadRadiusChunks; ++dx)
                {
                    const glm::ivec2 cp{cameraChunk.x + dx, cameraChunk.y + dy};
                    const bool shouldExist = chunkExistsAt(cp, sparsityDensityPercent);
                    const bool isResident  = world.chunkInBounds(cp);
                    if (shouldExist && !isResident)
                    {
                        const auto data = buildChunkData(chunkSize, cp);
                        world.addChunk(cp, std::span<const std::uint8_t>(data));
                    }
                    else if (!shouldExist && isResident)
                    {
                        world.removeChunk(cp);
                    }
                }
            // Eviction sweep: walk a wider square and remove anything outside the unload radius.
            const int sweep = unloadRadiusChunks + 2;
            for (int dy = -sweep; dy <= sweep; ++dy)
                for (int dx = -sweep; dx <= sweep; ++dx)
                {
                    if (std::abs(dx) <= unloadRadiusChunks && std::abs(dy) <= unloadRadiusChunks)
                        continue;
                    world.removeChunk({cameraChunk.x + dx, cameraChunk.y + dy});
                }
        }

        // ── UI ──────────────────────────────────────────────────────────
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Sparsemap");

        ImGui::Text("Camera: (%.0f, %.0f)", camera.x, camera.y);
        ImGui::Text("Chunks resident: %zu", world.chunkCount());

        const std::size_t bytesPerChunkCells = static_cast<std::size_t>(chunkSize.x * chunkSize.y);
        const std::size_t worldCellBytes     = world.chunkCount() * bytesPerChunkCells;
        char buf[64];
        formatBytes(buf, sizeof(buf), worldCellBytes);
        ImGui::Text("World cells: %s  (= %zu chunks × %zu bytes)", buf, world.chunkCount(), bytesPerChunkCells);

        ImGui::Separator();
        ImGui::Checkbox("Streaming around camera", &streamingOn);
        ImGui::SliderInt("Density (%)",            &sparsityDensityPercent, 0, 100);
        ImGui::SliderInt("Load radius (chunks)",   &loadRadiusChunks,   1, 16);
        ImGui::SliderInt("Unload radius (chunks)", &unloadRadiusChunks, 1, 32);
        if (loadRadiusChunks > unloadRadiusChunks) unloadRadiusChunks = loadRadiusChunks;

        ImGui::Separator();
        ImGui::Text("Manual addChunk");
        ImGui::InputInt("##addX", &manualAddX); ImGui::SameLine();
        ImGui::InputInt("##addY", &manualAddY); ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            const glm::ivec2 cp{manualAddX, manualAddY};
            const auto data = buildChunkData(chunkSize, cp);
            world.addChunk(cp, std::span<const std::uint8_t>(data));
        }

        ImGui::Text("Manual removeChunk");
        ImGui::InputInt("##remX", &manualRemoveX); ImGui::SameLine();
        ImGui::InputInt("##remY", &manualRemoveY); ImGui::SameLine();
        if (ImGui::Button("Remove"))
            world.removeChunk({manualRemoveX, manualRemoveY});

        ImGui::Separator();
        ImGui::Text("setCell editor (lazy-creates the chunk if missing)");
        ImGui::InputInt("World X", &editWorldX);
        ImGui::InputInt("World Y", &editWorldY);
        ImGui::SliderInt("Layer", &editLayer, 0, static_cast<int>(tileGraphics.size()) - 1);
        if (ImGui::Button("Paint"))
            world.setCell({editWorldX, editWorldY}, static_cast<std::uint8_t>(editLayer));

        ImGui::Separator();
        ImGui::TextWrapped(
            "Pan with WASD. A deterministic per-coord hash decides which chunks exist "
            "(see Density slider) — same coordinate, same answer, so revisiting a region "
            "restores the same scattered layout. Lower the Density to see more gaps. "
            "With streaming off, the world stops syncing; missing-chunk slots hide via "
            "chunkInBounds.");

        ImGui::End();
    }, controller);

    return 0;
}
