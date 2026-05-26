#include <cmath>
#include <vector>
#include <nothofagus.h>

// Custom-mesh demo:
//   * register a triangle and a pentagon as user meshes,
//   * draw them with a shared paletted texture,
//   * round-trip via getMesh(bellotaId) (read-only access to the underlying mesh),
//   * swap geometry mid-frame with setMesh(...).

namespace
{
    // Equilateral triangle inscribed in a circle of `radius` pixels, centered at origin.
    Nothofagus::Mesh makeTriangle(float radius)
    {
        const float twoPiOverThree = 2.0f * 3.14159265f / 3.0f;
        Nothofagus::Mesh mesh;
        for (int i = 0; i < 3; ++i)
        {
            const float angle = i * twoPiOverThree + 3.14159265f / 2.0f;
            const float x = radius * std::cos(angle);
            const float y = radius * std::sin(angle);
            const float u = 0.5f + 0.5f * std::cos(angle);
            const float v = 0.5f - 0.5f * std::sin(angle);
            mesh.vertices.push_back({x, y, u, v});
        }
        mesh.indices = {0, 1, 2};
        return mesh;
    }

    // Regular pentagon (triangle-fan around the center).
    Nothofagus::Mesh makePentagon(float radius)
    {
        const float twoPiOverFive = 2.0f * 3.14159265f / 5.0f;
        Nothofagus::Mesh mesh;
        mesh.vertices.push_back({0.0f, 0.0f, 0.5f, 0.5f});           // center
        for (int i = 0; i < 5; ++i)
        {
            const float angle = i * twoPiOverFive + 3.14159265f / 2.0f;
            const float x = radius * std::cos(angle);
            const float y = radius * std::sin(angle);
            const float u = 0.5f + 0.5f * std::cos(angle);
            const float v = 0.5f - 0.5f * std::sin(angle);
            mesh.vertices.push_back({x, y, u, v});
        }
        for (unsigned int i = 0; i < 5; ++i)
        {
            mesh.indices.push_back(0);
            mesh.indices.push_back(1 + i);
            mesh.indices.push_back(1 + ((i + 1) % 5));
        }
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

    // Register two user meshes.
    const Nothofagus::MeshId triangleMeshId = canvas.addMesh(makeTriangle(20.0f));
    const Nothofagus::MeshId pentagonMeshId = canvas.addMesh(makePentagon(20.0f));

    // One bellota for each custom mesh, plus a textured bellota to confirm the
    // auto-quad path keeps working unchanged.
    const Nothofagus::BellotaId triangleId =
        canvas.addBellota({{{60.0f,  75.0f}}, textureId, triangleMeshId});
    const Nothofagus::BellotaId pentagonId =
        canvas.addBellota({{{140.0f, 75.0f}}, textureId, pentagonMeshId});
    const Nothofagus::BellotaId quadId =
        canvas.addBellota({{{100.0f, 25.0f}}, textureId});

    // getMesh round-trip — confirms the auto-quad and user meshes are both queryable.
    spdlog::info("triangle mesh vertex count = {}", canvas.getMesh(triangleId).vertices.size());
    spdlog::info("auto-quad mesh vertex count = {}",  canvas.getMesh(quadId).vertices.size());

    float time = 0.0f;
    bool swap = false;
    bool prevSwap = false;

    auto update = [&](float dt)
    {
        time += dt;
        canvas.bellota(triangleId).transform().angle() = 0.05f * time;
        canvas.bellota(pentagonId).transform().angle() = -0.04f * time;

        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Custom meshes");
        ImGui::Text("Left: user triangle mesh");
        ImGui::Text("Right: user pentagon mesh");
        ImGui::Text("Bottom: implicit auto-quad");
        ImGui::Checkbox("Swap triangle <-> pentagon", &swap);
        ImGui::End();

        if (swap != prevSwap)
        {
            canvas.setMesh(triangleId, swap ? pentagonMeshId : triangleMeshId);
            canvas.setMesh(pentagonId, swap ? triangleMeshId : pentagonMeshId);
            prevSwap = swap;
        }
    };

    canvas.run(update);
    return 0;
}
