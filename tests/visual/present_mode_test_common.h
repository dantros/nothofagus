#pragma once

// Shared helpers for the present-mode regression tests. Split across two
// executables (pixel-invariance and sync-hazard) so each runs in its own process:
// the engine creates one Vulkan instance per Canvas, and vk-bootstrap caches the
// debug-messenger function pointer per process from the first instance, so mixing
// messenger and non-messenger instances in one process is unreliable. One concern
// per executable sidesteps that.

#include <canvas.h>
#include <present_mode.h>
#include <texture.h>
#include <bellota.h>

#include <array>

namespace PresentModeTest
{

constexpr std::array<Nothofagus::PresentMode, 3> kAllModes{
    Nothofagus::PresentMode::Fifo,
    Nothofagus::PresentMode::Mailbox,
    Nothofagus::PresentMode::Immediate,
};

inline const char* modeName(Nothofagus::PresentMode mode)
{
    switch (mode)
    {
    case Nothofagus::PresentMode::Fifo:      return "Fifo";
    case Nothofagus::PresentMode::Mailbox:   return "Mailbox";
    case Nothofagus::PresentMode::Immediate: return "Immediate";
    }
    return "?";
}

// Populate a fixed deterministic scene into an already-constructed canvas and run a
// few frames so GPU resources upload and present. (Canvas is non-movable, so callers
// construct it locally and hand it here by reference.)
inline void buildSceneAndTick(Nothofagus::Canvas& canvas, int ticks)
{
    Nothofagus::ColorPallete redPalette({{0, 0, 0, 0}, {1, 0, 0, 1}});
    Nothofagus::IndirectTexture redTex({3, 3}, {0, 0, 0, 0});
    redTex.setPallete(redPalette);
    redTex.setPixels({1, 1, 1, 1, 1, 1, 1, 1, 1});

    Nothofagus::ColorPallete bluePalette({{0, 0, 0, 0}, {0, 0, 1, 1}});
    Nothofagus::IndirectTexture blueTex({2, 2}, {0, 0, 0, 0});
    blueTex.setPallete(bluePalette);
    blueTex.setPixels({1, 1, 1, 1});

    auto redTexId  = canvas.addTexture(redTex);
    auto blueTexId = canvas.addTexture(blueTex);
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(2.5f, 2.5f)}, redTexId));
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(12.0f, 8.0f)}, blueTexId));

    for (int i = 0; i < ticks; ++i)
        canvas.tick(16.0f);
}

} // namespace PresentModeTest
