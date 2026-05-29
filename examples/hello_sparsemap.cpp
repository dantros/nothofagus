/// hello_sparsemap.cpp
/// Demonstrates the pooled `Sparsemap` + `SparsemapExplorer` sparse-tilemap pipeline —
/// the same chunk-pool optimization as `Tilemap`, but the world data lives in a
/// hash-map of chunks instead of a dense grid. There is no `mapSize`; the world is
/// unbounded and chunks exist only where added.
///
/// What this demo shows:
///   - An empty Sparsemap renders nothing (pool slots are hidden via `chunkInBounds`).
///   - "Streaming" simulation: as the camera pans, chunks inside a load radius are
///     `addChunk`'d (zero-init or with a stamped pattern); chunks outside an unload
///     radius are `removeChunk`'d. Memory scales with `chunkCount`, not with how far
///     you can travel.
///   - Manual `addChunk` / `removeChunk` at specific coords from the UI.
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

// Layer index 1..4 based on chunk coordinate (color cycles deterministically).
static std::uint8_t patternForChunk(glm::ivec2 chunkPos)
{
    const int sum = std::abs(chunkPos.x) + std::abs(chunkPos.y);
    return static_cast<std::uint8_t>(1 + (sum % 4));
}

// Pre-baked chunk data for a uniform solid layer (`chunkSize.x * chunkSize.y` bytes).
static std::vector<std::uint8_t> uniformChunkData(glm::ivec2 chunkSize, std::uint8_t layerIdx)
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(chunkSize.x * chunkSize.y), layerIdx);
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

    std::vector<std::vector<std::uint8_t>> tileGraphics{
        makeBorderedTile(tileSize, Pal::White),   // layer 0
        makeBorderedTile(tileSize, Pal::Red),     // layer 1
        makeBorderedTile(tileSize, Pal::Yellow),  // layer 2
        makeBorderedTile(tileSize, Pal::Blue),    // layer 3
        makeBorderedTile(tileSize, Pal::Green),   // layer 4
    };

    // Register the Sparsemap (world data) and a SparsemapExplorer (pooled renderer)
    // against the canvas. The explorer takes the SparsemapId it draws from. Nothing
    // renders until we start populating chunks below.
    Nothofagus::SparsemapId sparsemapId = canvas.addSparsemap(
        Nothofagus::Sparsemap(chunkSize, tileSize, palette,
            std::span<const std::vector<std::uint8_t>>(tileGraphics)));
    Nothofagus::SparsemapExplorerId explorerId =
        canvas.addSparsemapExplorer(Nothofagus::SparsemapExplorer(sparsemapId));

    // Seed a few chunks near the origin so the camera starts on populated content.
    {
        Nothofagus::Sparsemap& world = canvas.sparsemap(sparsemapId);
        for (int cy = -2; cy <= 2; ++cy)
            for (int cx = -2; cx <= 2; ++cx)
            {
                const auto data = uniformChunkData(chunkSize, patternForChunk({cx, cy}));
                world.addChunk({cx, cy}, std::span<const std::uint8_t>(data));
            }
    }

    // ── UI state ─────────────────────────────────────────────────────────
    glm::vec2 camera{0.0f, 0.0f};
    bool      streamingOn      = true;
    int       loadRadiusChunks = 4;       // chunks inside this radius (Chebyshev) get added
    int       unloadRadiusChunks = 6;     // chunks outside this radius get removed
    int       manualAddX = 5,  manualAddY = 0;
    int       manualRemoveX = -3, manualRemoveY = 0;
    int       editWorldX = 0, editWorldY = 24, editLayer = 4;  // setCell editor

    constexpr float panSpeed = 200.0f;
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

        // ── Streaming around the camera (simulated load/unload) ─────────
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
                    if (!world.chunkInBounds(cp))
                    {
                        const auto data = uniformChunkData(chunkSize, patternForChunk(cp));
                        world.addChunk(cp, std::span<const std::uint8_t>(data));
                    }
                }
            // Cheap eviction: walk a wider square and remove anything outside the unload radius.
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
        ImGui::SliderInt("Load radius (chunks)",   &loadRadiusChunks,   1, 16);
        ImGui::SliderInt("Unload radius (chunks)", &unloadRadiusChunks, 1, 32);
        if (loadRadiusChunks > unloadRadiusChunks) unloadRadiusChunks = loadRadiusChunks;

        ImGui::Separator();
        ImGui::Text("Manual addChunk");
        ImGui::InputInt("##addX", &manualAddX); ImGui::SameLine();
        ImGui::InputInt("##addY", &manualAddY); ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            const auto data = uniformChunkData(chunkSize, patternForChunk({manualAddX, manualAddY}));
            world.addChunk({manualAddX, manualAddY}, std::span<const std::uint8_t>(data));
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
            "Pan with WASD. Streaming loads chunks within Load radius of the camera and "
            "evicts those outside Unload radius. With streaming off, scrolling beyond the "
            "resident set shows empty space — slots covering missing chunks hide via chunkInBounds.");

        ImGui::End();
    }, controller);

    return 0;
}
