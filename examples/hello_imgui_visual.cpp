#include <cmath>
#include <numbers>
#include <vector>
#include <nothofagus.h>

// imguiVisual demo:
//   * draw a Visual's appearance inside an ImGui window (ImGui::Image),
//   * a palette-indexed, animated visual (proves the RTT palette/animation resolve),
//   * a custom-mesh (triangle) visual (proves the mesh-AABB fit — non-square),
//   * a standalone Visual{textureId} (no bellota, no mesh — engine synthesizes a quad),
//   * an opacity slider (one internal RTT reused; opacity applied as the image tint),
//   * a render target as the source (renderTo -> imguiVisual), proving an RTT texture is
//     sampled by imguiVisual exactly like a main-canvas bellota samples one.
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

    // Custom-mesh visual: a triangle (non-square AABB) sampling a 4-color quadrant
    // texture, so the texture mapping across the mesh's UVs is clearly visible.
    Nothofagus::IndirectTexture triTex({8, 8}, glm::vec4(0.0f));
    triTex.setPallete(pallete);
    {
        std::vector<std::uint8_t> pixels(64, 0);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                pixels[y * 8 + x] = static_cast<std::uint8_t>(1 + (x / 4) + 2 * (y / 4)); // 1..4 quadrants
        triTex.setPixels(pixels, 0);
    }
    Nothofagus::TextureId triTexId = canvas.addTexture(triTex);
    Nothofagus::MeshId triMeshId = canvas.addMesh(makeTriangle(16.0f));
    Nothofagus::BellotaId triBellotaId = canvas.addBellota({{{140.0f, 70.0f}}, triTexId, triMeshId});

    // Nearest-vs-Linear must use a DirectTexture (RGBA): paletted/IndirectTextures are
    // integer-indexed (texelFetch) and forced to Nearest, so magFilter has no effect on
    // them. A tiny 2x2 RGBA magnified shows hard pixels (Nearest) vs a smooth blend (Linear).
    Nothofagus::DirectTexture rgbaTex(glm::ivec2{2, 2});
    rgbaTex.setColor(0, 0, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f)); // red
    rgbaTex.setColor(1, 0, glm::vec4(0.2f, 1.0f, 0.3f, 1.0f)); // green
    rgbaTex.setColor(0, 1, glm::vec4(0.3f, 0.5f, 1.0f, 1.0f)); // blue
    rgbaTex.setColor(1, 1, glm::vec4(1.0f, 0.9f, 0.2f, 1.0f)); // yellow
    Nothofagus::TextureId nearestTexId = canvas.addTexture(rgbaTex); // default Nearest
    Nothofagus::TextureId linearTexId  = canvas.addTexture(rgbaTex);
    canvas.setTextureMagFilter(linearTexId, Nothofagus::TextureSampleMode::Linear);

    // Render-target source: draw an engine bellota into an off-screen RTT, then feed that
    // RTT's *texture* to imguiVisual. The expected order is the same one the standard
    // nested-RTT flow relies on — schedule renderTo(...) for the source each frame;
    // imguiVisual's internal pass is appended after all user RTT passes, so the source is
    // always rendered before it's sampled.
    Nothofagus::RenderTargetId sceneRtId = canvas.addRenderTarget({48, 48});
    canvas.setRenderTargetClearColor(sceneRtId, {0.05f, 0.06f, 0.12f, 1.0f});
    Nothofagus::TextureId sceneRtTexId = canvas.renderTargetTexture(sceneRtId);
    // A sprite living in the RTT's coordinate space (origin bottom-left, 48x48). It also
    // shows on the main canvas (renderTo dual-renders), mirroring hello_render_to_texture.
    Nothofagus::BellotaId sceneBellotaId = canvas.addBellota({{{24.0f, 24.0f}, 3.0f}, triTexId});

    float opacity = 1.0f;
    float scale   = 8.0f;
    float elapsedMs = 0.0f;

    canvas.run([&](float dt)
    {
        // Drive the animation by hand (changing the Visual's current layer).
        elapsedMs += dt;
        const std::size_t frame = static_cast<std::size_t>(elapsedMs / 180.0f) % kFrames;
        canvas.bellota(animBellotaId).currentLayer() = frame;

        // Spin the RTT sprite and schedule it into the off-screen target this frame, so the
        // imguiVisual sampling sceneRtTexId below has fresh pixels to read.
        canvas.bellota(sceneBellotaId).transform().angle() += dt * 0.05f;
        canvas.renderTo(sceneRtId, {sceneBellotaId});

        ImGui::Begin("Visuals in ImGui");

        ImGui::SliderFloat("scale", &scale, 1.0f, 20.0f);
        ImGui::SliderFloat("opacity", &opacity, 0.0f, 1.0f);
        ImGui::Separator();

        Nothofagus::Visual animVisual = canvas.bellota(animBellotaId).visual();
        animVisual.opacity() = opacity;

        ImGui::TextUnformatted("Natural = real size (DPI-scaled); Scaled rasterizes at the upscaled size;");
        ImGui::TextUnformatted("DevicePixels = exact physical pixels (1 texel -> 1 display pixel, ignores OS DPI):");
        ImGui::BeginGroup();
        ImGui::TextUnformatted("natural");
        canvas.imguiVisual(animVisual);                         // true 8x8 logical px (DPI-scaled)
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("scaled");
        canvas.imguiVisual(animVisual, Nothofagus::ImguiImageSize::Scaled{glm::vec2(scale)});    // crisp NxN
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("device 32px");
        canvas.imguiVisual(animVisual, Nothofagus::ImguiImageSize::DevicePixels{{32.0f, 32.0f}}); // exactly 32x32 screen px
        ImGui::EndGroup();

        ImGui::TextUnformatted("Custom-mesh (triangle) at an explicit logical size, Fit vs Stretch:");
        Nothofagus::Visual triVisual = canvas.bellota(triBellotaId).visual();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("logical Fit");
        canvas.imguiVisual(triVisual, Nothofagus::ImguiImageSize::LogicalPixels{{120.0f, 80.0f}, Nothofagus::ImguiImageFit::Fit});      // letterboxed, crisp edges
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("logical Stretch");
        canvas.imguiVisual(triVisual, Nothofagus::ImguiImageSize::LogicalPixels{{120.0f, 80.0f}, Nothofagus::ImguiImageFit::Stretch});  // fills, distorts
        ImGui::EndGroup();

        ImGui::TextUnformatted("RGBA texture magFilter (2x2 magnified) - Nearest vs Linear:");
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Nearest (default)");
        canvas.imguiVisual(Nothofagus::Visual{nearestTexId}, Nothofagus::ImguiImageSize::Scaled{glm::vec2(scale)});
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Linear");
        canvas.imguiVisual(Nothofagus::Visual{linearTexId}, Nothofagus::ImguiImageSize::Scaled{glm::vec2(scale)});
        ImGui::EndGroup();

        ImGui::TextUnformatted("Render target as source (renderTo -> imguiVisual) - same as a main bellota's RTT:");
        canvas.imguiVisual(Nothofagus::Visual{sceneRtTexId}, Nothofagus::ImguiImageSize::Scaled{glm::vec2(2.0f)});

        ImGui::End();
    });

    return 0;
}
