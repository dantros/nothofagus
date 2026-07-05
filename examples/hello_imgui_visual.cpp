#include <cmath>
#include <numbers>
#include <vector>
#include <nothofagus.h>

// Drawing engine Visuals inside ImGui — the registration model (registerImguiImage):
//   * register each Visual at a fixed size once, then draw it by id every frame,
//   * a palette-indexed, animated visual (updateImguiImage advances the frame; stable id),
//   * a custom-mesh (triangle) visual (proves the mesh-AABB fit — non-square),
//   * a standalone Visual{textureId} (no bellota, no mesh — engine synthesizes a quad),
//   * an opacity slider (fed through updateImguiImage; no re-warm),
//   * a scale slider via the imguiImage draw-size override — register once at a generous
//     resolution and downscale at draw time (a GPU sample of the fixed-res handle, stays crisp),
//   * a render target as the source (renderTo -> registered image), sampled like a main bellota.
//
// The entry point is a Visual, never a Bellota: placement (transform/depth) is
// meaningless inside an ImGui layout, so only the appearance is drawn.

namespace
{
    constexpr float kTwoPi  = 2.0f * std::numbers::pi_v<float>;
    constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;
    constexpr float kMaxScale = 20.0f; // the animated "scaled" image is rasterized at this size

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
    namespace Size = Nothofagus::ImguiImageSize;
    using Nothofagus::ImguiImageUnits;
    using Nothofagus::ImguiImageFit;

    Nothofagus::Canvas canvas({200, 140}, "imguiImage", {0.10f, 0.10f, 0.14f}, 5);

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
    const Nothofagus::Visual triVisual{triTexId, triMeshId};

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
    // RTT's *texture* to a registered image. renderTo(...) is scheduled each frame, and the
    // registered image's internal pass is appended after all user RTT passes, so the source
    // is always rendered before it's sampled.
    Nothofagus::RenderTargetId sceneRtId = canvas.addRenderTarget({48, 48});
    canvas.setRenderTargetClearColor(sceneRtId, {0.05f, 0.06f, 0.12f, 1.0f});
    Nothofagus::TextureId sceneRtTexId = canvas.renderTargetTexture(sceneRtId);
    Nothofagus::BellotaId sceneBellotaId = canvas.addBellota({{{24.0f, 24.0f}, 3.0f}, triTexId});

    // Register every image up front (before the loop): each handle is ready by its first
    // drawn frame, so nothing flashes an empty cell. The animated images are re-rendered
    // each frame via updateImguiImage; the rest are static registrations.
    const Nothofagus::ImguiImageId animNaturalId  = canvas.registerImguiImage(Nothofagus::Visual{animTexId}, Size::Natural{});
    const Nothofagus::ImguiImageId animScaledId   = canvas.registerImguiImage(Nothofagus::Visual{animTexId}, Size::Scaled{glm::vec2(kMaxScale)});
    const Nothofagus::ImguiImageId animNativeId   = canvas.registerImguiImage(Nothofagus::Visual{animTexId}, Size::Natural{ImguiImageUnits::Device});
    const Nothofagus::ImguiImageId animZoomId     = canvas.registerImguiImage(Nothofagus::Visual{animTexId}, Size::Scaled{glm::vec2(4.0f), ImguiImageUnits::Device});

    const Nothofagus::ImguiImageId triFitId       = canvas.registerImguiImage(triVisual, Size::Explicit{{120.0f, 80.0f}, ImguiImageFit::Fit});
    const Nothofagus::ImguiImageId triStretchId   = canvas.registerImguiImage(triVisual, Size::Explicit{{120.0f, 80.0f}, ImguiImageFit::Stretch});
    const Nothofagus::ImguiImageId triFitDevId    = canvas.registerImguiImage(triVisual, Size::Explicit{{120.0f, 80.0f}, ImguiImageFit::Fit, ImguiImageUnits::Device});
    const Nothofagus::ImguiImageId triStretchDevId= canvas.registerImguiImage(triVisual, Size::Explicit{{120.0f, 80.0f}, ImguiImageFit::Stretch, ImguiImageUnits::Device});

    const Nothofagus::ImguiImageId nearestId      = canvas.registerImguiImage(Nothofagus::Visual{nearestTexId}, Size::Scaled{glm::vec2(8.0f)});
    const Nothofagus::ImguiImageId linearId       = canvas.registerImguiImage(Nothofagus::Visual{linearTexId},  Size::Scaled{glm::vec2(8.0f)});
    const Nothofagus::ImguiImageId sceneRtImageId = canvas.registerImguiImage(Nothofagus::Visual{sceneRtTexId}, Size::Scaled{glm::vec2(2.0f)});

    // The animated registrations that share the same source content (advanced together).
    const Nothofagus::ImguiImageId animIds[] = {animNaturalId, animScaledId, animNativeId, animZoomId};

    float opacity = 1.0f;
    float scale   = 8.0f;
    float elapsedMs = 0.0f;

    // TODO(threaded): registered-image passes (registerImguiImage / imguiImage) are not
    // wired for the sim/render split yet (see THREADED_DIEGETIC_IMGUI.md). Parked on the
    // deprecated single-thread run(update, Controller&) until threaded support lands.
    Nothofagus::Controller deferredController;
    canvas.run([&](float dt)
    {
        // Advance the animation (current layer) and push it — plus the live opacity — onto
        // every registered view of the animated visual. Same size => no re-warm.
        elapsedMs += dt;
        const std::size_t frame = static_cast<std::size_t>(elapsedMs / 180.0f) % kFrames;
        Nothofagus::Visual animVisual{animTexId};
        animVisual.currentLayer() = frame;
        animVisual.opacity() = opacity;
        for (const Nothofagus::ImguiImageId id : animIds)
            canvas.updateImguiImage(id, animVisual);

        // Spin the RTT sprite and schedule it into the off-screen target this frame, so the
        // registered image sampling sceneRtTexId below has fresh pixels to read.
        canvas.bellota(sceneBellotaId).transform().angle() += dt * 0.05f;
        canvas.renderTo(sceneRtId, {sceneBellotaId});

        ImGui::Begin("Visuals in ImGui");

        ImGui::SliderFloat("draw scale", &scale, 1.0f, kMaxScale);
        ImGui::SliderFloat("opacity", &opacity, 0.0f, 1.0f);
        ImGui::Separator();

        ImGui::TextUnformatted("Size source x units. Natural = real size; Scaled = xN; Device units = exact");
        ImGui::TextUnformatted("physical px (1 texel -> 1 display pixel, ignores OS DPI):");
        ImGui::BeginGroup();
        ImGui::TextUnformatted("natural");
        canvas.imguiImage(animNaturalId);                       // Natural, Logical: true 8x8 logical px (DPI-scaled)
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("scaled (draw-size)");
        canvas.imguiImage(animScaledId, glm::vec2(8.0f * scale)); // registered at xMaxScale, drawn smaller (crisp downscale)
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("native (device)");
        canvas.imguiImage(animNativeId);                        // 1 texel -> 1 device px
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("zoom (device)");
        canvas.imguiImage(animZoomId);                          // crisp 4x in device px
        ImGui::EndGroup();

        ImGui::TextUnformatted("Custom-mesh (triangle) at an Explicit logical-px box, Fit vs Stretch:");
        ImGui::BeginGroup();
        ImGui::TextUnformatted("logical Fit");
        canvas.imguiImage(triFitId);      // letterboxed, crisp edges
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("logical Stretch");
        canvas.imguiImage(triStretchId);  // fills, distorts
        ImGui::EndGroup();

        ImGui::TextUnformatted("Same triangle at an Explicit device-px box (ignores OS DPI), Fit vs Stretch:");
        ImGui::BeginGroup();
        ImGui::TextUnformatted("device Fit");
        canvas.imguiImage(triFitDevId);      // letterboxed, exact device px
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("device Stretch");
        canvas.imguiImage(triStretchDevId);  // fills the device box, distorts
        ImGui::EndGroup();

        ImGui::TextUnformatted("RGBA texture magFilter (2x2 magnified) - Nearest vs Linear:");
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Nearest (default)");
        canvas.imguiImage(nearestId);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Linear");
        canvas.imguiImage(linearId);
        ImGui::EndGroup();

        ImGui::TextUnformatted("Render target as source (renderTo -> registered image):");
        canvas.imguiImage(sceneRtImageId);

        ImGui::End();
    }, deferredController);

    return 0;
}
