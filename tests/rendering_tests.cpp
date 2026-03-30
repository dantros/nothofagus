#include <catch2/catch_test_macros.hpp>
#include <canvas.h>
#include <texture.h>
#include <bellota.h>
#include "golden_image.h"
#include <string>
#include <cstdlib>

// Default golden directory is set by CMake. Override at runtime with GOLDEN_DIR env var
// to target a different set (e.g. golden_mesa/ for software rendering).
#ifndef NOTHOFAGUS_GOLDEN_DIR
    #define NOTHOFAGUS_GOLDEN_DIR "."
#endif

static std::string goldenDir()
{
    const char* env = std::getenv("GOLDEN_DIR");
    return (env != nullptr && env[0] != '\0') ? env : NOTHOFAGUS_GOLDEN_DIR;
}

static std::string goldenPath(const std::string& name)
{
    return goldenDir() + "/" + name + ".bin";
}

static bool shouldUpdateGolden()
{
    const char* env = std::getenv("UPDATE_GOLDEN");
    return env != nullptr && std::string(env) != "0";
}

static void checkAgainstGolden(const std::string& name, const Nothofagus::DirectTexture& screenshot)
{
    const std::string path = goldenPath(name);

    if (shouldUpdateGolden() || !GoldenImage::exists(path))
    {
        GoldenImage::save(path, screenshot);
        WARN("Golden file written: " + path);
        return;
    }

    Nothofagus::DirectTexture expected = GoldenImage::load(path);
    REQUIRE(screenshot.size() == expected.size());
    REQUIRE(GoldenImage::compare(screenshot, expected));
}


// ---------------------------------------------------------------------------
// Helper: create a headless canvas with a given size
// ---------------------------------------------------------------------------
static Nothofagus::Canvas makeCanvas(unsigned int width, unsigned int height)
{
    return Nothofagus::Canvas({width, height}, "test", {0.0f, 0.0f, 0.0f}, 1, 14, true);
}

// ---------------------------------------------------------------------------
// Test: single red square on black background
// ---------------------------------------------------------------------------
TEST_CASE("Single bellota renders correctly", "[rendering]")
{
    auto canvas = makeCanvas(8, 8);

    Nothofagus::ColorPallete palette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture tex({4, 4}, {0.0f, 0.0f, 0.0f, 0.0f});
    tex.setPallete(palette);
    tex.setPixels({
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    });
    auto texId = canvas.addTexture(tex);
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(4.0f, 4.0f)}, texId));

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("single_bellota", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: multiple bellotas at different positions
// ---------------------------------------------------------------------------
TEST_CASE("Multiple bellotas at different positions", "[rendering]")
{
    auto canvas = makeCanvas(12, 10);

    Nothofagus::ColorPallete redPalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture redTex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    redTex.setPallete(redPalette);
    redTex.setPixels({1, 1, 1, 1});

    Nothofagus::ColorPallete bluePalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 1.0f}
    });
    Nothofagus::IndirectTexture blueTex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    blueTex.setPallete(bluePalette);
    blueTex.setPixels({1, 1, 1, 1});

    auto redTexId  = canvas.addTexture(redTex);
    auto blueTexId = canvas.addTexture(blueTex);

    // Red at bottom-left corner (1,1)
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(2.0f, 2.0f)}, redTexId));
    // Blue at top-right area (9,7)
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(10.0f, 8.0f)}, blueTexId));

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("multiple_positions", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: depth ordering — front bellota occludes back bellota
// ---------------------------------------------------------------------------
TEST_CASE("Depth ordering occludes correctly", "[rendering]")
{
    auto canvas = makeCanvas(6, 6);

    Nothofagus::ColorPallete greenPalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture greenTex({4, 4}, {0.0f, 0.0f, 0.0f, 0.0f});
    greenTex.setPallete(greenPalette);
    greenTex.setPixels({
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    });

    Nothofagus::ColorPallete redPalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture redTex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    redTex.setPallete(redPalette);
    redTex.setPixels({1, 1, 1, 1});

    auto greenTexId = canvas.addTexture(greenTex);
    auto redTexId   = canvas.addTexture(redTex);

    // Green behind (depth -1), red in front (depth +1), overlapping at center
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(3.0f, 3.0f)}, greenTexId, -1));
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(3.0f, 3.0f)}, redTexId,   +1));

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("depth_ordering", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: bellota visibility toggle
// ---------------------------------------------------------------------------
TEST_CASE("Invisible bellota is not rendered", "[rendering]")
{
    auto canvas = makeCanvas(6, 6);

    Nothofagus::ColorPallete palette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture tex({4, 4}, {0.0f, 0.0f, 0.0f, 0.0f});
    tex.setPallete(palette);
    tex.setPixels({
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    });
    auto texId = canvas.addTexture(tex);
    auto bellotaId = canvas.addBellota(Nothofagus::Bellota({glm::vec2(3.0f, 3.0f)}, texId));

    // Make it invisible
    canvas.bellota(bellotaId).visible() = false;

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    // Should be entirely black
    checkAgainstGolden("invisible_bellota", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: non-black clear color fills the background
// ---------------------------------------------------------------------------
TEST_CASE("Clear color fills background", "[rendering]")
{
    Nothofagus::Canvas canvas({4, 4}, "test", {0.2f, 0.4f, 0.8f}, 1, 14, true);

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("clear_color", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: opacity — semi-transparent bellota over clear color
// ---------------------------------------------------------------------------
TEST_CASE("Semi-transparent bellota blends with background", "[rendering]")
{
    Nothofagus::Canvas canvas({4, 4}, "test", {0.0f, 0.0f, 1.0f}, 1, 14, true);

    Nothofagus::ColorPallete palette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture tex({4, 4}, {0.0f, 0.0f, 0.0f, 0.0f});
    tex.setPallete(palette);
    tex.setPixels({
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    });
    auto texId = canvas.addTexture(tex);
    auto bellotaId = canvas.addBellota(Nothofagus::Bellota({glm::vec2(2.0f, 2.0f)}, texId));
    canvas.bellota(bellotaId).opacity() = 0.5f;

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("opacity_blend", canvas.takeScreenshot());
}
