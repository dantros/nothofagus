#include <cmath>
#include <vector>
#include <nothofagus.h>

// ImGui image registry demo (Phase 1 of the imgui-image work):
//   * registerImguiImage(visual, sizeSpec) -> a stable ImguiImageId, allocated up front,
//   * the id resolves to a plain ImTextureID usable in raw ImGui::Image(...),
//   * the convenience canvas.imguiImage(id) draws it (honoring magFilter + opacity),
//   * no one-frame warm-up: a registered image is ready before it is first displayed,
//   * updateImguiImage(id, visual) drives a live animation without re-warming the handle,
//   * a draw-size override downsizes the draw only (a GPU sample of the fixed-res handle),
//   * unregisterImguiImage(id) frees it.
//
// See hello_imgui_visual.cpp for the same registration model exercised across every size
// mode (Natural / Scaled / Device, Explicit Fit/Stretch on a mesh, Nearest/Linear, RTT source).

namespace
{
    constexpr std::size_t kFrames = 4;

    Nothofagus::IndirectTexture makeAnimatedTexture(const Nothofagus::ColorPallete& pallete)
    {
        Nothofagus::IndirectTexture tex({8, 8}, glm::vec4(0.0f), kFrames);
        tex.setPallete(pallete);
        for (std::size_t frame = 0; frame < kFrames; ++frame)
        {
            std::vector<std::uint8_t> pixels(64, 0);
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    if ((x + y + static_cast<int>(frame)) % 4 == 0)
                        pixels[y * 8 + x] = static_cast<std::uint8_t>(1 + ((x + frame) % 4));
            tex.setPixels(pixels, frame);
        }
        return tex;
    }
}

int main()
{
    Nothofagus::Canvas canvas({200, 140}, "imguiImage registry", {0.10f, 0.10f, 0.14f}, 5);

    Nothofagus::ColorPallete pallete{
        {0.0f, 0.0f, 0.0f, 0.0f},  // 0: transparent
        {1.0f, 0.3f, 0.3f, 1.0f},  // 1: red
        {0.3f, 1.0f, 0.4f, 1.0f},  // 2: green
        {0.4f, 0.6f, 1.0f, 1.0f},  // 3: blue
        {1.0f, 0.9f, 0.3f, 1.0f},  // 4: yellow
    };

    Nothofagus::TextureId iconTexId = canvas.addTexture(makeAnimatedTexture(pallete));

    // Register the same visual at two fixed sizes BEFORE the loop. Their handles are ready
    // by the first displayed frame, so neither flashes an empty cell.
    const Nothofagus::ImguiImageId smallId =
        canvas.registerImguiImage(Nothofagus::Visual{iconTexId},
                                  Nothofagus::ImguiImageSize::Scaled{glm::vec2(3.0f)});
    const auto registerBig = [&]{
        return canvas.registerImguiImage(Nothofagus::Visual{iconTexId},
                                         Nothofagus::ImguiImageSize::Scaled{glm::vec2(10.0f)});
    };
    Nothofagus::ImguiImageId bigId = registerBig();

    float elapsedMs = 0.0f;
    float drawScale = 1.0f;
    bool  registered = true;

    canvas.run([&](float dt)
    {
        // Advance the animation by re-registering the current layer onto the big image's id.
        // The id (and its ImTextureID) stays stable across the update — no re-warm.
        elapsedMs += dt;
        const std::size_t frame = static_cast<std::size_t>(elapsedMs / 180.0f) % kFrames;
        if (registered)
        {
            Nothofagus::Visual animated{iconTexId};
            animated.currentLayer() = frame;
            canvas.updateImguiImage(bigId, animated);
        }

        ImGui::Begin("Registered images");

        ImGui::TextUnformatted("Pre-registered -> no one-frame warm-up.");
        ImGui::Separator();

        // 1) Convenience draw via the id.
        ImGui::TextUnformatted("imguiImage(smallId):");
        canvas.imguiImage(smallId);

        // 2) Raw ImGui::Image with the id's handle + size: the id is a first-class handle.
        ImGui::TextUnformatted("raw ImGui::Image(handle, size):");
        if (canvas.isImguiImageReady(smallId))
        {
            const glm::vec2 size = canvas.imguiImageSize(smallId);
            ImGui::Image(static_cast<ImTextureID>(canvas.imguiImageHandle(smallId)),
                         ImVec2(size.x, size.y));
        }

        // 3) Animated registration + a draw-size override (downscale of the fixed-res handle).
        ImGui::Separator();
        ImGui::SliderFloat("draw scale", &drawScale, 0.25f, 1.0f);
        ImGui::TextUnformatted("animated (updateImguiImage), draw-size overridden:");
        if (registered)
        {
            const glm::vec2 natural = canvas.imguiImageSize(bigId);
            canvas.imguiImage(bigId, natural * drawScale);
        }

        ImGui::Separator();
        if (ImGui::Button(registered ? "unregister animated" : "re-register animated"))
        {
            if (registered)
                canvas.unregisterImguiImage(bigId);
            else
                bigId = registerBig();
            registered = not registered;
        }

        ImGui::End();
    });

    return 0;
}
