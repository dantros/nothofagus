// Finding 1 — present mode never changes rendered pixels.
//
// The screenshot is identical across Fifo / Mailbox / Immediate and run-to-run.
// Meaningful on windowed Vulkan (real swapchain); trivially true under headless
// Vulkan (present mode is a no-op there), so it is correct to run in both builds.
//
// Intended for LOCAL / windowed runs (the CI lane builds the headless backend where
// present mode has no effect). See tests/visual/README.md.

#include <catch2/catch_test_macros.hpp>

#include "present_mode_test_common.h"

#include <cstdint>
#include <span>
#include <vector>

namespace
{

std::vector<std::uint8_t> screenshotBytes(Nothofagus::PresentMode mode)
{
    Nothofagus::Canvas canvas(
        {15, 10}, "present_mode_test", {0.0f, 0.0f, 0.0f}, 1, 14,
        /*headless=*/true, mode);
    PresentModeTest::buildSceneAndTick(canvas, /*ticks=*/4);
    canvas.requestScreenshot();
    canvas.tick(16.0f); // render one more frame to capture it
    Nothofagus::DirectTexture shot = *canvas.retrieveScreenshot();
    Nothofagus::TextureData data = shot.generateTextureData();
    std::span<std::uint8_t> span = data.getDataSpan();
    return std::vector<std::uint8_t>(span.begin(), span.end());
}

} // namespace

TEST_CASE("present mode never changes rendered pixels (Finding 1)", "[present_mode][visual]")
{
    std::vector<std::uint8_t> reference;
    for (Nothofagus::PresentMode mode : PresentModeTest::kAllModes)
    {
        INFO("present mode: " << PresentModeTest::modeName(mode));
        const std::vector<std::uint8_t> first  = screenshotBytes(mode);
        const std::vector<std::uint8_t> second = screenshotBytes(mode);

        REQUIRE_FALSE(first.empty());
        // Run-to-run determinism within a mode.
        CHECK(first == second);

        if (reference.empty())
            reference = first;
        else
            CHECK(first == reference); // identical across present modes
    }
}
