// Threaded sanitizer smoke — a self-closing workload for running TSan / ASan+UBSan against
// the SwiftShader Vulkan headless backend (see tools/sanitizers/run_sanitizers.sh). Mesa is
// buggy under sanitizers, so we render on the deterministic CPU rasterizer instead.
//
// It drives the threaded run(update, ui) path and touches the cross-thread surfaces most
// likely to race:
//   - sim-thread scene mutation (bellota transform)
//   - sim-thread asset create/destroy (mode-aware wrappers + deferred GPU free)
//   - threaded ImGui (a ui window built on the sim-UI context, cloned to the render thread)
//   - the scheduled-screenshot channel (requestScreenshot / retrieveScreenshot marshaling)
// then closes itself after a fixed number of frames so the process exits with a real return
// code (rather than being killed at a timeout).
//
// Kept small on purpose: SwiftShader under TSan serializes on its worker pool, so a tiny
// device resolution keeps the run tractable. pixelSize > 1 still exercises the device-
// resolution screenshot path that crashed the headless backend before #114.
//
// Tunables via env: NOTHO_SMOKE_FRAMES (default 24), NOTHO_SMOKE_PIXELSIZE (default 2).

#include <nothofagus.h>
#include <imgui.h>
#include <cstdio>
#include <cstdlib>
#include <optional>

static int envInt(const char* name, int fallback)
{
    if (const char* v = std::getenv(name)) { const int n = std::atoi(v); if (n > 0) return n; }
    return fallback;
}

int main()
{
    const int totalFrames = envInt("NOTHO_SMOKE_FRAMES", 24);
    const int pixelSize    = envInt("NOTHO_SMOKE_PIXELSIZE", 2); // > 1 exercises device-resolution capture

    Nothofagus::Canvas canvas({32, 24}, "sanitizer-threaded-smoke", {0.1f, 0.15f, 0.2f},
                              static_cast<unsigned int>(pixelSize));

    Nothofagus::IndirectTexture sprite({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    sprite.setPallete({{0, 0, 0, 0}, {1, 0.3f, 0.2f, 1}});
    sprite.setPixels({1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
                      1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
                      1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
                      1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1});
    auto texId = canvas.addTexture(sprite);
    auto bellotaId = canvas.addBellota({{{16.0f, 12.0f}, 2.0f}, texId});

    canvas.setTargetFps(120.0f); // keep sim/render cadences aligned

    int frame = 0;
    bool requested = false, gotShot = false;
    glm::ivec2 shotSize{0, 0};
    std::optional<Nothofagus::BellotaId> churn;

    const int requestAt = totalFrames / 2;

    auto update = [&](float)
    {
        ++frame;
        canvas.bellota(bellotaId).transform().location().x = 16.0f + 4.0f * ((frame % 8) - 4) / 4.0f;

        // Sim-thread asset create/destroy churn (mode-aware wrappers + deferred free).
        if (frame % 6 == 3)
            churn = canvas.addBellota({{{4.0f, 4.0f}, 1.0f}, texId});
        else if (frame % 6 == 0 && churn)
            { canvas.removeBellota(*churn); churn.reset(); }

        // Scheduled screenshot across the sim/render boundary (device-resolution capture).
        if (frame == requestAt && !requested) { requested = true; canvas.requestScreenshot(); }
        if (std::optional<Nothofagus::DirectTexture> shot = canvas.retrieveScreenshot())
            if (shot->size().x > 0) { gotShot = true; shotSize = shot->size(); }

        if (frame >= totalFrames) canvas.close();
    };
    auto ui = [&](float) { ImGui::Begin("smoke"); ImGui::Text("f%d", frame); ImGui::End(); };

    canvas.run(update, ui);

    std::printf("smoke done: frames=%d gotShot=%d shot=%dx%d\n", frame, gotShot ? 1 : 0, shotSize.x, shotSize.y);
    return (frame >= totalFrames && gotShot) ? 0 : 1;
}
