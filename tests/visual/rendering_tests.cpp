#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <canvas.h>
#include <texture.h>
#include <text.h>
#include <bellota.h>
#include <mesh.h>
#include <markdown_renderer.h>
#include <imgui_overlay.h>
#include <imgui.h>
#include "direct_texture_io.h"
#include "direct_texture_compare.h"
#include <string>
#include <cstdlib>
#include <filesystem>
#include <iostream>

// Path to the SwiftShader ICD JSON, baked in by CMake when
// NOTHOFAGUS_FETCH_SWIFTSHADER fetched a prebuilt. Empty when unavailable.
#ifndef NOTHOFAGUS_SWIFTSHADER_ICD
    #define NOTHOFAGUS_SWIFTSHADER_ICD ""
#endif

// Portable set/unset of an environment variable (POSIX setenv/unsetenv vs Windows _putenv_s).
static void setEnvVar(const char* key, const char* value)
{
#if defined(_WIN32)
    _putenv_s(key, value ? value : "");
#else
    if (value != nullptr && value[0] != '\0')
        ::setenv(key, value, 1);
    else
        ::unsetenv(key);
#endif
}

// Forces the Vulkan loader to a specific ICD before the first Canvas (and thus
// the first Vulkan call) is constructed, so the same binary renders with either
// SwiftShader (deterministic CPU) or the system GPU. Selected by the
// NOTHOFAGUS_RENDER_BACKEND env var: "swiftshader" | "gpu" | "auto" (default).
//
// Registered as a Catch2 listener so testRunStarting runs once before any test
// case constructs a Canvas — including the few that build one without makeCanvas.
struct RenderBackendSelector : Catch::EventListenerBase
{
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo&) override
    {
        const char* env = std::getenv("NOTHOFAGUS_RENDER_BACKEND");
        const std::string backend = (env != nullptr && env[0] != '\0') ? env : "auto";

        if (backend == "swiftshader")
        {
            const std::string icd = NOTHOFAGUS_SWIFTSHADER_ICD;
            if (icd.empty())
            {
                std::cerr << "[render-backend] NOTHOFAGUS_RENDER_BACKEND=swiftshader requested but no "
                             "SwiftShader ICD was built in (configure with NOTHOFAGUS_FETCH_SWIFTSHADER=ON "
                             "on a supported platform); using the default Vulkan ICD instead.\n";
                return;
            }
            setEnvVar("VK_ICD_FILENAMES", icd.c_str());
            setEnvVar("VK_DRIVER_FILES", icd.c_str());
            std::cerr << "[render-backend] swiftshader ICD: " << icd << "\n";
        }
        else if (backend == "gpu")
        {
            // Clear any inherited ICD override (e.g. from the Visual Tests Explorer)
            // so the loader enumerates the real system GPU drivers.
            setEnvVar("VK_ICD_FILENAMES", "");
            setEnvVar("VK_DRIVER_FILES", "");
            std::cerr << "[render-backend] gpu (system Vulkan ICDs)\n";
        }
        // "auto": leave the loader environment untouched.
    }
};
CATCH_REGISTER_LISTENER(RenderBackendSelector)

// Default golden directory is set by CMake. Override at runtime with GOLDEN_DIR env var
// to target a different set (e.g. golden_mesa/ for software rendering).
#ifndef NOTHOFAGUS_GOLDEN_DIR
    #define NOTHOFAGUS_GOLDEN_DIR "."
#endif
// Directory the "actual" renders are dumped into (for the Visual Tests Explorer tool).
#ifndef NOTHOFAGUS_ACTUAL_DIR
    #define NOTHOFAGUS_ACTUAL_DIR "."
#endif

// Comparison thresholds. Goldens are produced by deterministic CPU rendering
// (SwiftShader in CI), so these defaults are tight; loosen via env vars to
// tolerate minor rasterizer rounding on other backends.
static constexpr std::uint8_t kDefaultPerChannelTolerance = 2;
static constexpr std::size_t  kDefaultMaxDifferingPixels   = 0;

static std::string goldenDir()
{
    const char* env = std::getenv("GOLDEN_DIR");
    return (env != nullptr && env[0] != '\0') ? env : NOTHOFAGUS_GOLDEN_DIR;
}

static std::string actualDir()
{
    const char* env = std::getenv("ACTUAL_DIR");
    return (env != nullptr && env[0] != '\0') ? env : NOTHOFAGUS_ACTUAL_DIR;
}

static std::string goldenPath(const std::string& name)
{
    return goldenDir() + "/" + name + ".png";
}

static std::string actualPath(const std::string& name)
{
    return actualDir() + "/" + name + ".png";
}

static bool shouldUpdateGolden()
{
    const char* env = std::getenv("UPDATE_GOLDEN");
    return env != nullptr && std::string(env) != "0";
}

static bool shouldDumpActual()
{
    const char* env = std::getenv("DUMP_ACTUAL");
    return env != nullptr && std::string(env) != "0";
}

static std::uint8_t perChannelTolerance()
{
    const char* env = std::getenv("GOLDEN_TOLERANCE");
    return (env != nullptr && env[0] != '\0')
        ? static_cast<std::uint8_t>(std::strtoul(env, nullptr, 10))
        : kDefaultPerChannelTolerance;
}

static std::size_t maxDifferingPixels()
{
    const char* env = std::getenv("GOLDEN_MAX_DIFF_PIXELS");
    return (env != nullptr && env[0] != '\0')
        ? static_cast<std::size_t>(std::strtoull(env, nullptr, 10))
        : kDefaultMaxDifferingPixels;
}

// Writes the actual render next to the goldens so the visual_tests_explorer tool can
// show golden / actual / diff. Best-effort: never fails the test.
static void dumpActual(const std::string& name, const Nothofagus::DirectTexture& screenshot)
{
    try
    {
        std::error_code ec;
        std::filesystem::create_directories(actualDir(), ec);
        Nothofagus::TestHelpers::save(actualPath(name), screenshot);
    }
    catch (const std::exception&)
    {
        // Diagnostic output only; not a test failure.
    }
}

static void checkAgainstGolden(const std::string& name, const Nothofagus::DirectTexture& screenshot)
{
    const std::string path = goldenPath(name);

    if (shouldUpdateGolden() || !std::filesystem::exists(path))
    {
        std::error_code ec;
        std::filesystem::create_directories(goldenDir(), ec);
        Nothofagus::TestHelpers::save(path, screenshot);
        WARN("Golden file written: " + path);
        return;
    }

    Nothofagus::DirectTexture expected = Nothofagus::TestHelpers::load(path);

    const Nothofagus::TestHelpers::ComparisonResult result =
        Nothofagus::TestHelpers::compare(screenshot, expected, perChannelTolerance(), maxDifferingPixels());

    if (!result.withinTolerance || shouldDumpActual())
        dumpActual(name, screenshot);

    INFO("golden:           " << path);
    INFO("sizeMismatch:     " << (result.sizeMismatch ? "yes" : "no"));
    INFO("differingPixels:  " << result.differingPixels << " (max " << maxDifferingPixels() << ")");
    INFO("maxChannelDelta:  " << static_cast<int>(result.maxChannelDelta)
                              << " (tolerance " << static_cast<int>(perChannelTolerance()) << ")");
    INFO("meanChannelDelta: " << result.meanChannelDelta);

    REQUIRE_FALSE(result.sizeMismatch);
    REQUIRE(result.withinTolerance);
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

// ---------------------------------------------------------------------------
// Test: custom triangle mesh renders correctly
// ---------------------------------------------------------------------------
TEST_CASE("Custom mesh renders correctly", "[rendering][mesh]")
{
    auto canvas = makeCanvas(10, 10);

    Nothofagus::ColorPallete palette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    });
    Nothofagus::IndirectTexture tex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    tex.setPallete(palette);
    tex.setPixels({1, 1, 1, 1});
    auto texId = canvas.addTexture(tex);

    // Upward-pointing triangle, centered on origin, ~3px radius. UVs sample
    // the (uniform) white texture so the triangle silhouette is what matters.
    Nothofagus::Mesh triangle;
    triangle.vertices = {
        {{ 0.0f,  3.0f}, {0.5f, 0.0f}},
        {{ 3.0f, -3.0f}, {1.0f, 1.0f}},
        {{-3.0f, -3.0f}, {0.0f, 1.0f}},
    };
    triangle.indices = {0, 1, 2};

    auto meshId = canvas.addMesh(triangle);
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(5.0f, 5.0f)}, texId, meshId));

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("custom_mesh_triangle", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: auto-quad regenerates when setTexture swaps to a differently-sized texture.
// Renders a small bellota first, then rebinds it to a larger texture and verifies
// the auto-quad rebuilt to match the new size.
// ---------------------------------------------------------------------------
TEST_CASE("Auto-quad regenerates on setTexture", "[rendering][mesh]")
{
    auto canvas = makeCanvas(10, 10);

    Nothofagus::ColorPallete palette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    });

    Nothofagus::IndirectTexture smallTex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    smallTex.setPallete(palette);
    smallTex.setPixels({1, 1, 1, 1});

    Nothofagus::IndirectTexture largeTex({6, 6}, {0.0f, 0.0f, 0.0f, 0.0f});
    largeTex.setPallete(palette);
    largeTex.setPixels({
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1,
    });

    auto smallTexId = canvas.addTexture(smallTex);
    auto largeTexId = canvas.addTexture(largeTex);

    // Bellota starts on the 2x2 texture (auto-quad sized 2x2).
    auto bellotaId = canvas.addBellota(Nothofagus::Bellota({glm::vec2(5.0f, 5.0f)}, smallTexId));

    // Swap to the 6x6 texture — the auto-quad must regenerate to that size.
    canvas.setTexture(bellotaId, largeTexId);

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("auto_quad_resized_after_setTexture", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: setMesh swaps the bellota's geometry mid-life. Registers two
// distinguishable user meshes (triangle vs left-half quad) and captures the
// post-swap frame.
// ---------------------------------------------------------------------------
TEST_CASE("setMesh swaps geometry mid-frame", "[rendering][mesh]")
{
    auto canvas = makeCanvas(10, 10);

    Nothofagus::ColorPallete palette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    });
    Nothofagus::IndirectTexture tex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    tex.setPallete(palette);
    tex.setPixels({1, 1, 1, 1});
    auto texId = canvas.addTexture(tex);

    // Triangle pointing up.
    Nothofagus::Mesh triangle;
    triangle.vertices = {
        {{ 0.0f,  3.0f}, {0.5f, 0.0f}},
        {{ 3.0f, -3.0f}, {1.0f, 1.0f}},
        {{-3.0f, -3.0f}, {0.0f, 1.0f}},
    };
    triangle.indices = {0, 1, 2};

    // Solid square offset to the lower-left of the bellota origin so the
    // post-swap render is visibly different from the triangle.
    Nothofagus::Mesh leftHalfQuad;
    leftHalfQuad.vertices = {
        {{-3.0f, -3.0f}, {0.0f, 1.0f}},
        {{ 0.0f, -3.0f}, {1.0f, 1.0f}},
        {{ 0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-3.0f,  0.0f}, {0.0f, 0.0f}},
    };
    leftHalfQuad.indices = {0, 1, 2, 2, 3, 0};

    auto triangleMeshId = canvas.addMesh(triangle);
    auto squareMeshId   = canvas.addMesh(leftHalfQuad);

    auto bellotaId = canvas.addBellota(
        Nothofagus::Bellota({glm::vec2(5.0f, 5.0f)}, texId, triangleMeshId)
    );

    // Swap geometry before any frame ticks — the bellota should render as the
    // square, not the triangle.
    canvas.setMesh(bellotaId, squareMeshId);

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("setMesh_swap_geometry", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// ImGui overlay bars (header + footer)
//
// Equivalent of the engine_header / engine_footer overlay bars: a dark bar
// pinned to the top of the game viewport and one pinned to the bottom, each with
// horizontally + vertically centered text. The bars are positioned through
// computeImguiOverlayViewport(), the single framebuffer-px -> ImGui-display
// conversion point. These goldens lock the overlay layout (bar height, top/bottom
// placement, text centering) so the recurring "fix one case, break another"
// scaling churn is caught across the GL/VK, GLFW/SDL3, win/linux matrix. The
// HiDPI/pillarbox math itself is pinned deterministically by the nonvisual
// imgui_overlay_tests (headless contentScale is always 1.0).
// ---------------------------------------------------------------------------
// ImGui 1.92's dynamic font atlas rasterizes glyphs on demand and uploads the
// font texture incrementally, so a freshly-constructed canvas needs several
// frames before every glyph is resident and the rendered output is stable. A
// cold single-case run with too few warmup frames captures a half-populated
// atlas (and the exact frame it settles is process-state dependent), so all
// ImGui goldens warm up by this many frames before the captured frame.
static constexpr int kImguiWarmupFrames = 16;

static void drawOverlayBars(Nothofagus::Canvas& canvas,
                            const std::string& headerText,
                            const std::string& footerText)
{
    // Position + size entirely through the public Canvas overlay API so this
    // golden also guards the live accessors. Bar height comes from
    // imguiScaledFontSize() (base * content scale) so screen overlays grow with
    // the standard UI on HiDPI. At the default scale of 1 this equals
    // imguiBaseFontSize(), so the basic/pillarbox goldens are unchanged; the
    // imgui_overlay_scaled case exercises scale != 1 via setContentScaleOverride.
    const Nothofagus::ImguiOverlayRect rect = canvas.imguiOverlayViewport();
    const float barHeight = canvas.imguiScaledFontSize() * 1.875f;
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    auto drawBar = [&](const char* id, float yTop, const std::string& text)
    {
        ImGui::SetNextWindowPos(ImVec2(rect.x, yTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(rect.width, barHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        // Bars are thinner than ImGui's default WindowMinSize (32 px, and
        // 32 * contentScale on HiDPI); without this the windows inflate to that
        // minimum — the top header shows the full inflated height while the
        // bottom footer's surplus is clipped off-screen, so they look uneven.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
        ImGui::Begin(id, nullptr, flags);
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::SetCursorPos(ImVec2((rect.width - textSize.x) * 0.5f, (barHeight - textSize.y) * 0.5f));
        ImGui::TextUnformatted(text.c_str());
        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(1);
    };

    drawBar("##test_header", rect.y, headerText);
    // An empty footerText draws the header only, leaving the bottom of the
    // viewport visible (used by the scaled case to inspect the lower region).
    if (!footerText.empty())
        drawBar("##test_footer", rect.y + rect.height - barHeight, footerText);
}

TEST_CASE("ImGui overlay bars render centered", "[rendering][imgui]")
{
    auto canvas = makeCanvas(100, 100);

    for (int i = 0; i < kImguiWarmupFrames; ++i)
        canvas.tick(16.0f, [&](float) { drawOverlayBars(canvas, "HEADER", "FOOTER"); });

    checkAgainstGolden("imgui_overlay_basic", canvas.takeScreenshot());
}

TEST_CASE("ImGui overlay bars track pillarbox offset", "[rendering][imgui]")
{
    // 200x100 framebuffer, 100x100 logical canvas -> pillarbox (viewport.x = 50).
    // The screenshot captures only the 100x100 game viewport; the bars must fill
    // its full width, proving they tracked the horizontal offset.
    Nothofagus::Canvas canvas({200, 100}, "test", {0.0f, 0.0f, 0.0f}, 1, 14, true);
    canvas.setScreenSize({100, 100});

    for (int i = 0; i < kImguiWarmupFrames; ++i)
        canvas.tick(16.0f, [&](float) { drawOverlayBars(canvas, "HEADER", "FOOTER"); });

    checkAgainstGolden("imgui_overlay_pillarbox", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Markdown tables: columns size proportionally and long cells wrap inside their
// own column. The canvas is deliberately narrow so the long cells must wrap,
// pinning the table layout (proportional columns, in-column wrapping, header).
// ---------------------------------------------------------------------------
TEST_CASE("Markdown tables render with wrapped columns", "[rendering][imgui]")
{
    auto canvas = makeCanvas(220, 160);

    Nothofagus::MarkdownRenderer markdown(canvas);
    markdown.setStyle(canvas.defaultMarkdownStyle(14.0f));   // bake before ticking

    static constexpr const char* kTable =
        "| field | notes |\n"
        "|-------|-------|\n"
        "| short | A long cell that must wrap inside its column. |\n"
        "| again | Second long row sharing the column width. |\n";

    for (int i = 0; i < kImguiWarmupFrames; ++i)
        canvas.tick(16.0f, [&](float) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(220.0f, 160.0f), ImGuiCond_Always);
            ImGui::Begin("md", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            markdown.print(kTable);
            ImGui::End();
        });

    checkAgainstGolden("markdown_tables", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// OS DPI scaling of the standard-UI (main) context.
//
// setContentScaleOverride() makes the content scale deterministic and
// independent of the host monitor, so these goldens can prove that the main
// context honors the OS scale: at 2x both the font AND the widget metrics
// (window/frame padding, the framed button, the checkbox) render twice as large
// within the same fixed headless framebuffer. The HiDPI policy math is also
// pinned by the nonvisual imgui_scale tests (headless contentScale is 1.0).
// ---------------------------------------------------------------------------
static void drawStandardUi(Nothofagus::Canvas& canvas)
{
    // Fill the game viewport with one borderless panel of native-style widgets,
    // positioned/sized through the overlay API (top-left origin, points).
    const Nothofagus::ImguiOverlayRect rect = canvas.imguiOverlayViewport();
    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##std_ui", nullptr, flags);
    ImGui::TextUnformatted("UI");
    ImGui::Button("Btn");
    bool checkOn = true; // fixed state -> deterministic render (no input in headless)
    ImGui::Checkbox("On", &checkOn);
    ImGui::End();
}

TEST_CASE("ImGui standard UI at content scale 1x", "[rendering][imgui]")
{
    auto canvas = makeCanvas(120, 90);
    canvas.setContentScaleOverride(1.0f);

    for (int i = 0; i < kImguiWarmupFrames; ++i)
        canvas.tick(16.0f, [&](float) { drawStandardUi(canvas); });

    checkAgainstGolden("imgui_dpi_scale_1x", canvas.takeScreenshot());
}

TEST_CASE("ImGui standard UI scales font and metrics at 2x", "[rendering][imgui]")
{
    auto canvas = makeCanvas(240, 180);
    canvas.setContentScaleOverride(2.0f);

    for (int i = 0; i < kImguiWarmupFrames; ++i)
        canvas.tick(16.0f, [&](float) { drawStandardUi(canvas); });

    checkAgainstGolden("imgui_dpi_scale_2x", canvas.takeScreenshot());
}

TEST_CASE("ImGui overlay bars scale with content scale", "[rendering][imgui]")
{
    auto canvas = makeCanvas(100, 100);
    canvas.setContentScaleOverride(2.0f);

    // Header only (empty footer) so the bottom of the viewport stays visible.
    for (int i = 0; i < kImguiWarmupFrames; ++i)
        canvas.tick(16.0f, [&](float) { drawOverlayBars(canvas, "HEADER", ""); });

    checkAgainstGolden("imgui_overlay_scaled", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: tile-map text — makeTextTexture renders a single-line string as one
// bellota / one draw call (glyph atlas as layers, string as the cell grid).
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap text renders a single line", "[rendering][text]")
{
    auto canvas = makeCanvas(40, 10);

    // White glyphs on a transparent background; "Hi" = 2 cells -> 16x8 world.
    Nothofagus::IndirectTexture textTex = Nothofagus::makeTextTexture(
        "Hi", Nothofagus::FontType::Basic, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f});
    auto texId = canvas.addTexture(textTex);
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(20.0f, 5.0f)}, texId));

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("tilemap_text_single_line", canvas.takeScreenshot());
}

// ---------------------------------------------------------------------------
// Test: multi-line tile-map text — '\n' splits into tile-map rows, line 0 on top.
// Also exercises setText, which rewrites only the cell grid in place.
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap text renders multiple lines", "[rendering][text]")
{
    auto canvas = makeCanvas(40, 30);

    // 2x2 cells -> 16x16 world. Built with placeholder text, then re-spelled via
    // setText to the same dimensions (the cheap map-only update path).
    Nothofagus::IndirectTexture textTex = Nothofagus::makeTextTexture(
        "..\n..", Nothofagus::FontType::Basic, {0.2f, 1.0f, 0.4f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f});
    auto texId = canvas.addTexture(textTex);
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(20.0f, 15.0f)}, texId));

    Nothofagus::setText(std::get<Nothofagus::IndirectTexture>(canvas.texture(texId)), "AB\nCD");
    canvas.markTextureAsDirty(texId);

    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    checkAgainstGolden("tilemap_text_multi_line", canvas.takeScreenshot());
}
