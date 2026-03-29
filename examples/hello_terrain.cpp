#include <nothofagus.h>
#include <vector>
#include <cmath>

int main()
{
    Nothofagus::Canvas canvas(
        {256, 240},
        "Hello Terrain",
        {0.3f, 0.5f, 0.8f},   // sky-blue background
        4
    );

    // ── Terrain colour texture (checkerboard green/brown) ──────────────────────
    constexpr int textureSize = 4;
    Nothofagus::IndirectTexture terrainTexture({textureSize, textureSize}, {0, 0, 0, 255});
    terrainTexture.setPallete(Nothofagus::ColorPallete{{
        {0.2f, 0.6f, 0.1f, 1.0f},   // index 0: green
        {0.5f, 0.3f, 0.1f, 1.0f},   // index 1: brown
    }});
    terrainTexture.setPixels({
        0, 1, 0, 1,
        1, 0, 1, 0,
        0, 1, 0, 1,
        1, 0, 1, 0,
    });
    Nothofagus::TextureId terrainTexId = canvas.addTexture(terrainTexture);

    // ── Heightmap: 16 × 16 grid with a central hill ───────────────────────────
    constexpr std::size_t gridRows    = 16;
    constexpr std::size_t gridColumns = 16;
    std::vector<float> heights(gridRows * gridColumns, 0.0f);

    for (std::size_t row = 0; row < gridRows; ++row)
    {
        for (std::size_t col = 0; col < gridColumns; ++col)
        {
            const float normX = static_cast<float>(col) / static_cast<float>(gridColumns - 1) - 0.5f;
            const float normZ = static_cast<float>(row) / static_cast<float>(gridRows    - 1) - 0.5f;
            const float distance = std::sqrt(normX * normX + normZ * normZ);
            // Gaussian hill in the centre
            heights[row * gridColumns + col] = std::exp(-distance * distance * 8.0f);
        }
    }

    Nothofagus::HeightmapTerrain terrain(
        heights,
        gridRows,
        gridColumns,
        /*worldWidth=*/  256.0f,
        /*worldDepth=*/  240.0f,
        /*maximumHeight=*/ 40.0f,
        terrainTexId
    );
    canvas.addHeightmapTerrain(terrain);

    // ── Camera ─────────────────────────────────────────────────────────────────
    canvas.camera().position()           = {128.0f, 80.0f, 220.0f};
    canvas.camera().target()             = {128.0f,  0.0f, 120.0f};
    canvas.camera().fieldOfViewDegrees() = 60.0f;

    // ── Character sprite (world-space billboard) ───────────────────────────────
    Nothofagus::IndirectTexture characterTexture({8, 8}, {0, 0, 0, 0});
    characterTexture.setPallete(Nothofagus::ColorPallete{{
        {0.0f, 0.0f, 0.0f, 0.0f},   // index 0: transparent
        {0.9f, 0.7f, 0.3f, 1.0f},   // index 1: body
        {0.8f, 0.3f, 0.1f, 1.0f},   // index 2: outline
    }});
    characterTexture.setPixels({
        0, 0, 2, 2, 2, 2, 0, 0,
        0, 2, 1, 1, 1, 1, 2, 0,
        0, 2, 1, 2, 2, 1, 2, 0,
        0, 2, 1, 1, 1, 1, 2, 0,
        0, 0, 2, 1, 1, 2, 0, 0,
        0, 0, 2, 1, 1, 2, 0, 0,
        0, 2, 1, 0, 0, 1, 2, 0,
        2, 1, 0, 0, 0, 0, 1, 2,
    });
    Nothofagus::TextureId characterTexId = canvas.addTexture(characterTexture);

    // Helper: nearest-neighbour sample of the heightmap in world space
    auto sampleTerrainHeight = [&](float worldX, float worldZ) -> float
    {
        const float normCol = worldX / 256.0f * static_cast<float>(gridColumns - 1);
        const float normRow = worldZ / 240.0f * static_cast<float>(gridRows    - 1);
        const int col = std::clamp(static_cast<int>(normCol), 0, static_cast<int>(gridColumns) - 1);
        const int row = std::clamp(static_cast<int>(normRow), 0, static_cast<int>(gridRows)    - 1);
        return heights[row * gridColumns + col] * 40.0f;
    };

    // Initial character Y: terrain height at spawn + half sprite height so it sits on the surface
    const float initialTerrainY = sampleTerrainHeight(128.0f, 120.0f); // = 40 at hill peak
    Nothofagus::WorldBellotaId characterId = canvas.addWorldBellota(
        {/*position=*/{128.0f, initialTerrainY + 10.0f, 120.0f}, /*size=*/{16.0f, 20.0f}, characterTexId}
    );

    // ── HUD sprite (screen-space, always on top) ───────────────────────────────
    Nothofagus::IndirectTexture hudTexture({16, 16}, {0, 0, 0, 0});
    hudTexture.setPallete(Nothofagus::ColorPallete{{
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
    }});
    // Simple red border
    std::vector<std::uint8_t> hudPixels(16 * 16, 0);
    for (int i = 0; i < 16; ++i)
    {
        hudPixels[i]              = 1; // top row
        hudPixels[15 * 16 + i]   = 1; // bottom row
        hudPixels[i * 16]         = 1; // left column
        hudPixels[i * 16 + 15]   = 1; // right column
    }
    hudTexture.setPixels(hudPixels.begin(), hudPixels.end());
    Nothofagus::TextureId hudTexId = canvas.addTexture(hudTexture);

    Nothofagus::BellotaId hudId = canvas.addBellota(
        Nothofagus::Bellota(Nothofagus::Transform(glm::vec2{8.0f, 8.0f}), hudTexId)
    );
    canvas.bellota(hudId).depthOffset() = 127; // always on top of other 2D

    // ── Input: orbit the camera and move the character ─────────────────────────
    Nothofagus::Controller controller;
    float cameraAngle = 0.0f;
    float characterX  = 128.0f;
    const float characterZ  = 120.0f;

    bool movingLeft  = false;
    bool movingRight = false;
    controller.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Press},   [&]() { movingLeft  = true;  });
    controller.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Release}, [&]() { movingLeft  = false; });
    controller.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Press},   [&]() { movingRight = true;  });
    controller.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Release}, [&]() { movingRight = false; });

    canvas.run([&](float deltaTimeMs)
    {
        const float deltaTimeSec = deltaTimeMs / 1000.0f;

        // Slowly orbit the camera
        cameraAngle += 20.0f * deltaTimeSec;
        const float radians = glm::radians(cameraAngle);
        const float orbitRadius = 150.0f;
        canvas.camera().position().x = 128.0f + orbitRadius * std::sin(radians);
        canvas.camera().position().z = 120.0f + orbitRadius * std::cos(radians);
        canvas.camera().target() = {128.0f, 0.0f, 120.0f};

        // Move character left/right
        if (movingLeft)
            characterX -= 40.0f * deltaTimeSec;
        if (movingRight)
            characterX += 40.0f * deltaTimeSec;

        // Snap character Y to terrain surface so it's always visible
        const float terrainY = sampleTerrainHeight(characterX, characterZ);
        canvas.worldBellota(characterId).position().x = characterX;
        canvas.worldBellota(characterId).position().y = terrainY + 10.0f; // +10 = half sprite height
        canvas.worldBellota(characterId).position().z = characterZ;

        ImGui::SetNextWindowPos({4.0f, 4.0f});
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
        ImGui::Text("A/D — move character");
        ImGui::Text("Camera orbits automatically");
        ImGui::End();
    }, controller);

    return 0;
}
