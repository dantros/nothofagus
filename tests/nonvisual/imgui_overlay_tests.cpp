#include <catch2/catch_test_macros.hpp>
#include <imgui_overlay.h>

using Nothofagus::computeImguiOverlayViewport;
using Nothofagus::ImguiOverlayRect;
using Nothofagus::effectiveContentScale;

// These tests pin down the framebuffer-pixels -> ImGui-display-points conversion
// that overlay bars (header/footer) depend on. They are backend-independent and
// deterministically cover the HiDPI (DisplayFramebufferScale != 1) and
// pillarbox/letterbox cases that a headless golden render cannot reproduce
// (headless contentScale is always 1.0). Convention: gameViewport is framebuffer
// pixels (y-up); displaySize is ImGui points; framebuffer px = displaySize * scale.

// ---------------------------------------------------------------------------
// fbScale == 1, no letterbox: identity (viewport fills the framebuffer, points
// == pixels).
// ---------------------------------------------------------------------------
TEST_CASE("Overlay viewport is identity at unit scale", "[imgui_overlay]")
{
    const ImguiOverlayRect rect = computeImguiOverlayViewport(
        0, 0, 100, 100,
        100.0f, 100.0f,
        1.0f, 1.0f);

    REQUIRE(rect.x      == 0.0f);
    REQUIRE(rect.y      == 0.0f);
    REQUIRE(rect.width  == 100.0f);
    REQUIRE(rect.height == 100.0f);
}

// ---------------------------------------------------------------------------
// HiDPI: a full-framebuffer viewport maps to the full (smaller) point-space
// display. fb px = display points * scale, so a viewport equal to the whole
// framebuffer maps back to the whole display.
// ---------------------------------------------------------------------------
TEST_CASE("Overlay viewport divides by DisplayFramebufferScale", "[imgui_overlay]")
{
    SECTION("scale 2")
    {
        // 200x200 framebuffer px == 100x100 display points at scale 2.
        const ImguiOverlayRect rect = computeImguiOverlayViewport(
            0, 0, 200, 200,
            100.0f, 100.0f,
            2.0f, 2.0f);

        REQUIRE(rect.x      == 0.0f);
        REQUIRE(rect.y      == 0.0f);
        REQUIRE(rect.width  == 100.0f);
        REQUIRE(rect.height == 100.0f);
    }

    SECTION("scale 1.5")
    {
        // 300x150 framebuffer px == 200x100 display points at scale 1.5.
        const ImguiOverlayRect rect = computeImguiOverlayViewport(
            0, 0, 300, 150,
            200.0f, 100.0f,
            1.5f, 1.5f);

        REQUIRE(rect.x      == 0.0f);
        REQUIRE(rect.y      == 0.0f);
        REQUIRE(rect.width  == 200.0f);
        REQUIRE(rect.height == 100.0f);
    }
}

// ---------------------------------------------------------------------------
// Pillarbox: a wide framebuffer leaves the game viewport offset to the right.
// The overlay must track that horizontal offset (this is the bug the bars hit
// when the window is enlarged).
// ---------------------------------------------------------------------------
TEST_CASE("Overlay viewport tracks pillarbox offset", "[imgui_overlay]")
{
    SECTION("unit scale")
    {
        // 200x100 framebuffer (== display points), 100x100 game viewport
        // centered -> x = 50.
        const ImguiOverlayRect rect = computeImguiOverlayViewport(
            50, 0, 100, 100,
            200.0f, 100.0f,
            1.0f, 1.0f);

        REQUIRE(rect.x      == 50.0f);
        REQUIRE(rect.y      == 0.0f);
        REQUIRE(rect.width  == 100.0f);
        REQUIRE(rect.height == 100.0f);
    }

    SECTION("pillarbox + HiDPI scale 2")
    {
        // 200x100 framebuffer px == 100x50 display points at scale 2; viewport
        // offset 50px -> 25 points.
        const ImguiOverlayRect rect = computeImguiOverlayViewport(
            50, 0, 100, 100,
            100.0f, 50.0f,
            2.0f, 2.0f);

        REQUIRE(rect.x      == 25.0f);
        REQUIRE(rect.y      == 0.0f);
        REQUIRE(rect.width  == 50.0f);
        REQUIRE(rect.height == 50.0f);
    }
}

// ---------------------------------------------------------------------------
// Letterbox: a tall framebuffer leaves black bands top and bottom. The game
// viewport sits in the middle (viewport.y from the bottom), and the overlay's
// top edge must land on the upper band boundary, not at y = 0.
// ---------------------------------------------------------------------------
TEST_CASE("Overlay viewport flips letterbox y to top-left origin", "[imgui_overlay]")
{
    // 100x100 framebuffer (== display points), 100x50 game viewport centered
    // vertically: viewport.y (from bottom) = 25, height = 50. Top band height =
    // 25, so the overlay top in ImGui (top-down) coords is y = 25.
    const ImguiOverlayRect rect = computeImguiOverlayViewport(
        0, 25, 100, 50,
        100.0f, 100.0f,
        1.0f, 1.0f);

    REQUIRE(rect.x      == 0.0f);
    REQUIRE(rect.y      == 25.0f);
    REQUIRE(rect.width  == 100.0f);
    REQUIRE(rect.height == 50.0f);
}

// ---------------------------------------------------------------------------
// Degenerate scale values fall back to 1 instead of dividing by zero.
// ---------------------------------------------------------------------------
TEST_CASE("Overlay viewport guards non-positive scale", "[imgui_overlay]")
{
    const ImguiOverlayRect rect = computeImguiOverlayViewport(
        0, 0, 80, 60,
        80.0f, 60.0f,
        0.0f, 0.0f);

    REQUIRE(rect.x      == 0.0f);
    REQUIRE(rect.y      == 0.0f);
    REQUIRE(rect.width  == 80.0f);
    REQUIRE(rect.height == 60.0f);
}

// ---------------------------------------------------------------------------
// effectiveContentScale: the policy that drives main-context DPI scaling.
// An explicit override wins; otherwise the backend's OS scale; non-positive
// inputs (zeroed override, backend reporting 0) fall back to 1.0 so the UI
// never collapses. These pin the policy the visual goldens rely on, with no
// render backend needed (headless contentScale is always 1.0).
// ---------------------------------------------------------------------------
TEST_CASE("effectiveContentScale prefers the override", "[imgui_scale]")
{
    REQUIRE(effectiveContentScale(2.0f, 1.0f) == 2.0f);   // override wins over backend
    REQUIRE(effectiveContentScale(0.75f, 3.0f) == 0.75f); // even a smaller override wins
}

TEST_CASE("effectiveContentScale falls back to the backend scale", "[imgui_scale]")
{
    REQUIRE(effectiveContentScale(0.0f, 1.5f) == 1.5f);   // no override -> backend value
    REQUIRE(effectiveContentScale(0.0f, 1.0f) == 1.0f);
}

TEST_CASE("effectiveContentScale guards non-positive inputs", "[imgui_scale]")
{
    REQUIRE(effectiveContentScale(0.0f, 0.0f) == 1.0f);    // both absent -> 1.0
    REQUIRE(effectiveContentScale(-2.0f, 0.0f) == 1.0f);   // negative override ignored
    REQUIRE(effectiveContentScale(0.0f, -1.0f) == 1.0f);   // negative backend ignored
}

// ---------------------------------------------------------------------------
// Scaled overlay bar height: screen-space overlays size their bar height from
// the scaled base font (imguiScaledFontSize == base * effectiveScale) so they
// grow with the standard UI on HiDPI. This mirrors Canvas::imguiScaledFontSize.
// ---------------------------------------------------------------------------
TEST_CASE("Scaled overlay bar height tracks the content scale", "[imgui_scale]")
{
    constexpr float baseFontSize = 14.0f;
    constexpr float barRatio     = 1.875f;

    auto barHeight = [&](float scale) { return baseFontSize * scale * barRatio; };

    REQUIRE(barHeight(effectiveContentScale(0.0f, 1.0f)) == 14.0f * 1.875f);        // scale 1
    REQUIRE(barHeight(effectiveContentScale(2.0f, 1.0f)) == 14.0f * 2.0f * 1.875f); // override 2
    REQUIRE(barHeight(effectiveContentScale(0.0f, 1.5f)) == 14.0f * 1.5f * 1.875f); // backend 1.5
}
