#include <cmath>
#include <numbers>
#include <vector>
#include <nothofagus.h>

// imguiVisual demo:
//   * draw a Visual's appearance inside an ImGui window (ImGui::Image),
//   * a palette-indexed, animated visual (proves the RTT palette/animation resolve),
//   * a custom-mesh (triangle) visual (proves the mesh-AABB fit — non-square),
//   * a standalone Visual{textureId} (no bellota, no mesh — engine synthesizes a quad),
//   * an opacity slider (one internal RTT reused; opacity applied as the image tint).
//
// The entry point is a Visual, never a Bellota: placement (transform/depth) is
// meaningless inside an ImGui layout, so only the appearance is drawn.

namespace
{
    constexpr float kTwoPi  = 2.0f * std::numbers::pi_v<float>;
    constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;

    Nothofagus::Mesh makeTriangle(float radius)
    {
        Nothofagus::Mesh mesh;
        for (int i = 0; i < 3; ++i)
        {
            const float angle = i * (kTwoPi / 3.0f) + kHalfPi;
            mesh.vertices.push_back({
                {radius * std::cos(angle), radius * std::sin(angle)},
                {0.5f + 0.5f * std::cos(angle), 0.5f - 0.5f * std::sin(angle)}});
        }
        mesh.indices = {0, 1, 2};
        return mesh;
    }
}

int main()
{
    Nothofagus::Canvas canvas({200, 140}, "imguiVisual", {0.10f, 0.10f, 0.14f}, 5);

    Nothofagus::ColorPallete pallete{
        {0.0f, 0.0f, 0.0f, 0.0f},  // 0: transparent
        {1.0f, 0.3f, 0.3f, 1.0f},  // 1: red
        {0.3f, 1.0f, 0.4f, 1.0f},  // 2: green
        {0.4f, 0.6f, 1.0f, 1.0f},  // 3: blue
        {1.0f, 0.9f, 0.3f, 1.0f},  // 4: yellow
    };

    // Animated paletted texture: 8x8, 4 layers (frames). A diagonal that shifts per layer.
    constexpr std::size_t kFrames = 4;
    Nothofagus::IndirectTexture animTex({8, 8}, glm::vec4(0.0f), kFrames);
    animTex.setPallete(pallete);
    for (std::size_t frame = 0; frame < kFrames; ++frame)
    {
        std::vector<std::uint8_t> pixels(64, 0);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                if ((x + y + static_cast<int>(frame)) % 4 == 0)
                    pixels[y * 8 + x] = static_cast<std::uint8_t>(1 + ((x + frame) % 4));
        animTex.setPixels(pixels, frame);
    }
    Nothofagus::TextureId animTexId = canvas.addTexture(animTex);
    Nothofagus::BellotaId animBellotaId = canvas.addBellota({{{60.0f, 70.0f}}, animTexId});

    // Custom-mesh visual: a triangle (non-square AABB) sharing a small paletted texture.
    Nothofagus::IndirectTexture triTex({8, 8}, glm::vec4(0.0f));
    triTex.setPallete(pallete);
    {
        std::vector<std::uint8_t> pixels(64, 2);
        triTex.setPixels(pixels, 0);
    }
    Nothofagus::TextureId triTexId = canvas.addTexture(triTex);
    Nothofagus::MeshId triMeshId = canvas.addMesh(makeTriangle(16.0f));
    Nothofagus::BellotaId triBellotaId = canvas.addBellota({{{140.0f, 70.0f}}, triTexId, triMeshId});

    float opacity = 1.0f;
    float elapsedMs = 0.0f;

    canvas.run([&](float dt)
    {
        // Drive the animation by hand (changing the Visual's current layer).
        elapsedMs += dt;
        const std::size_t frame = static_cast<std::size_t>(elapsedMs / 180.0f) % kFrames;
        canvas.bellota(animBellotaId).currentLayer() = frame;

        ImGui::Begin("Visuals in ImGui");

        ImGui::TextUnformatted("Animated paletted visual:");
        Nothofagus::Visual animVisual = canvas.bellota(animBellotaId).visual();
        animVisual.opacity() = opacity;
        canvas.imguiVisual(animVisual, {96.0f, 96.0f});

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Custom-mesh visual:");
        canvas.imguiVisual(canvas.bellota(triBellotaId).visual(), {96.0f, 96.0f});
        ImGui::EndGroup();

        ImGui::TextUnformatted("Standalone Visual{textureId} (frame 0):");
        canvas.imguiVisual(Nothofagus::Visual{animTexId}, {64.0f, 64.0f});

        ImGui::SliderFloat("opacity", &opacity, 0.0f, 1.0f);
        ImGui::End();
    });

    return 0;
}
