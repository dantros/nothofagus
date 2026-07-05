#include <cmath>
#include <numbers>
#include <vector>
#include <nothofagus.h>

// Custom-mesh demo:
//   * register a triangle and a pentagon as user meshes,
//   * draw them with a shared paletted texture,
//   * round-trip via mesh(bellotaId) (read-only access to the underlying mesh),
//   * swap geometry mid-frame with setMesh(...),
//   * use the 4-arg constructor to pin a foreground bellota with an explicit depth offset,
//   * release a transient mesh explicitly via removeMesh after rebinding off it.

namespace
{
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;

    // Equilateral triangle inscribed in a circle of `radius` pixels, centered at origin.
    Nothofagus::Mesh makeTriangle(float radius)
    {
        Nothofagus::Mesh mesh;
        for (int i = 0; i < 3; ++i)
        {
            const float angle = i * (kTwoPi / 3.0f) + kHalfPi;
            const float x = radius * std::cos(angle);
            const float y = radius * std::sin(angle);
            const float u = 0.5f + 0.5f * std::cos(angle);
            const float v = 0.5f - 0.5f * std::sin(angle);
            mesh.vertices.push_back({{x, y}, {u, v}});
        }
        mesh.indices = {0, 1, 2};
        return mesh;
    }

    // Regular pentagon, decomposed as a triangle fan around the center vertex.
    Nothofagus::Mesh makePentagon(float radius)
    {
        Nothofagus::Mesh mesh;
        mesh.vertices.push_back({{0.0f, 0.0f}, {0.5f, 0.5f}});           // fan center
        for (int i = 0; i < 5; ++i)
        {
            const float angle = i * (kTwoPi / 5.0f) + kHalfPi;
            const float x = radius * std::cos(angle);
            const float y = radius * std::sin(angle);
            const float u = 0.5f + 0.5f * std::cos(angle);
            const float v = 0.5f - 0.5f * std::sin(angle);
            mesh.vertices.push_back({{x, y}, {u, v}});
        }
        for (unsigned int i = 0; i < 5; ++i)
        {
            mesh.indices.push_back(0);
            mesh.indices.push_back(1 + i);
            mesh.indices.push_back(1 + ((i + 1) % 5));
        }
        return mesh;
    }

    // Axis-aligned diamond (square rotated 45°) used as a transient mesh
    // that the demo registers, swaps onto a bellota, then swaps off and
    // removes explicitly via canvas.removeMesh.
    Nothofagus::Mesh makeDiamond(float radius)
    {
        Nothofagus::Mesh mesh;
        mesh.vertices = {
            {{ 0.0f,  radius}, {0.5f, 0.0f}},
            {{ radius, 0.0f }, {1.0f, 0.5f}},
            {{ 0.0f, -radius}, {0.5f, 1.0f}},
            {{-radius, 0.0f }, {0.0f, 0.5f}},
        };
        mesh.indices = {0, 1, 2, 0, 2, 3};
        return mesh;
    }

    // Axis-aligned square of side 2 * halfSide, centered at origin.
    Nothofagus::Mesh makeQuad(float halfSide)
    {
        Nothofagus::Mesh mesh;
        mesh.vertices = {
            {{-halfSide, -halfSide}, {0.0f, 1.0f}},
            {{ halfSide, -halfSide}, {1.0f, 1.0f}},
            {{ halfSide,  halfSide}, {1.0f, 0.0f}},
            {{-halfSide,  halfSide}, {0.0f, 0.0f}},
        };
        mesh.indices = {0, 1, 2, 0, 2, 3};
        return mesh;
    }
}

int main()
{
    spdlog::info("Hello custom meshes!");

    Nothofagus::Canvas canvas({200, 150}, "Hello Mesh", {0.1f, 0.1f, 0.15f}, 4);

    // Simple 4-color palette: index 0 transparent, 1-3 visible colours.
    Nothofagus::ColorPallete pallete{
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.7f, 0.2f, 1.0f},
        {0.2f, 0.8f, 1.0f, 1.0f},
        {0.9f, 0.3f, 0.4f, 1.0f},
    };

    Nothofagus::IndirectTexture texture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    texture.setPallete(pallete)
           .setPixels({
                1,1,2,2,2,2,1,1,
                1,2,2,3,3,2,2,1,
                2,2,3,3,3,3,2,2,
                2,3,3,1,1,3,3,2,
                2,3,3,1,1,3,3,2,
                2,2,3,3,3,3,2,2,
                1,2,2,3,3,2,2,1,
                1,1,2,2,2,2,1,1,
           });
    const Nothofagus::TextureId textureId = canvas.addTexture(texture);

    // Register three user meshes (the fourth — the diamond — is registered and
    // removed later as part of the explicit-cleanup demonstration below).
    const Nothofagus::MeshId triangleMeshId = canvas.addMesh(makeTriangle(20.0f));
    const Nothofagus::MeshId pentagonMeshId = canvas.addMesh(makePentagon(20.0f));
    const Nothofagus::MeshId quadMeshId     = canvas.addMesh(makeQuad(18.0f));

    // One bellota per custom mesh. The triangle uses the 4-arg
    // Bellota(Transform, TextureId, MeshId, depthOffset) constructor with a
    // positive depth offset so it always sorts above the pentagon when their
    // bounding circles overlap.
    constexpr std::int8_t triangleDepthOffset = 1;
    const Nothofagus::BellotaId triangleId =
        canvas.addBellota({{{60.0f,  75.0f}}, textureId, triangleMeshId, triangleDepthOffset});
    const Nothofagus::BellotaId pentagonId =
        canvas.addBellota({{{140.0f, 75.0f}}, textureId, pentagonMeshId});
    const Nothofagus::BellotaId quadId =
        canvas.addBellota({{{100.0f, 25.0f}}, textureId, quadMeshId});

    // mesh(bellotaId) round-trip — confirms user meshes are queryable through the bellota handle.
    spdlog::info("triangle mesh vertex count = {}", canvas.mesh(triangleId).vertices.size());
    spdlog::info("quad mesh vertex count = {}",     canvas.mesh(quadId).vertices.size());

    // Explicit-cleanup demonstration: register a transient diamond mesh,
    // briefly bind the pentagon bellota to it, swap back to the original
    // pentagon, and explicitly call removeMesh on the diamond. Without the
    // explicit removeMesh the diamond would still get GC'd automatically
    // on the next frame — this just shows the synchronous user-driven path.
    {
        const Nothofagus::MeshId diamondMeshId = canvas.addMesh(makeDiamond(20.0f));
        canvas.setMesh(pentagonId, diamondMeshId);   // pentagon bellota now draws as a diamond
        canvas.setMesh(pentagonId, pentagonMeshId);  // …then back to the pentagon
        canvas.removeMesh(diamondMeshId);            // safe now: nothing references it
    }

    float time = 0.0f;
    bool swap = false;
    bool prevSwap = false;

    // Game logic — runs on the sim thread (no ImGui here). Reads `swap`, which the ui
    // writes; both callbacks run on the sim thread, so a plain bool is safe.
    auto update = [&](float dt)
    {
        time += dt;
        canvas.bellota(triangleId).transform().angle() = 0.05f * time;
        canvas.bellota(pentagonId).transform().angle() = -0.04f * time;

        if (swap != prevSwap)
        {
            canvas.setMesh(triangleId, swap ? pentagonMeshId : triangleMeshId);
            canvas.setMesh(pentagonId, swap ? triangleMeshId : pentagonMeshId);
            prevSwap = swap;
        }
    };

    // Interactive ImGui — runs on the sim-UI context (sim thread), cloned to the render thread.
    auto ui = [&](float)
    {
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Custom meshes");
        ImGui::Text("Left: user triangle mesh");
        ImGui::Text("Right: user pentagon mesh");
        ImGui::Text("Bottom: user quad mesh");
        ImGui::Checkbox("Swap triangle <-> pentagon", &swap);
        ImGui::End();
    };

    canvas.run(update, ui);
    return 0;
}
