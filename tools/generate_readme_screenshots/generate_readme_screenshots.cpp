/// generate_readme_screenshots.cpp
///
/// Re-runnable headless tool that renders one representative static frame per
/// advertised feature and saves it as media/feature_<name>.png for embedding in
/// README.md. Each scene is adapted from the matching example under examples/,
/// stripped of interactivity. Run after building with
/// -DNOTHOFAGUS_BUILD_README_SCREENSHOTS=ON; re-running overwrites the PNGs in place.
///
/// Capture mechanics: a hidden GLFW window on the real GPU (headless=true) is driven
/// a few frames with canvas.tick(...), then canvas.takeScreenshot() reads the last
/// swapped frame (game viewport, top-to-bottom RGBA). ImGui panels are captured as
/// long as their window is anchored inside the game viewport (i.e. at (0,0)).

#include <nothofagus.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <direct_texture_io.h> // Nothofagus::TestHelpers::save

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace
{
const std::string kMediaDir = README_SCREENSHOTS_MEDIA_DIR;

/// Nearest-neighbor integer upscale of a top-to-bottom RGBA buffer. Keeps pixel-art
/// edges crisp so the small native-resolution captures display large in the README.
Nothofagus::DirectTexture nearestUpscale(const Nothofagus::TextureData& src, int factor)
{
    const int srcWidth = static_cast<int>(src.width());
    const int srcHeight = static_cast<int>(src.height());
    const int dstWidth = srcWidth * factor;
    const int dstHeight = srcHeight * factor;

    Nothofagus::TextureData dst(dstWidth, dstHeight, 1);
    std::span<std::uint8_t> srcBytes = src.getDataSpan();
    std::span<std::uint8_t> dstBytes = dst.getDataSpan();

    for (int y = 0; y < dstHeight; ++y)
    {
        const int srcY = y / factor;
        for (int x = 0; x < dstWidth; ++x)
        {
            const int srcX = x / factor;
            const std::size_t s = (static_cast<std::size_t>(srcY) * srcWidth + srcX) * 4;
            const std::size_t d = (static_cast<std::size_t>(y) * dstWidth + x) * 4;
            dstBytes[d + 0] = srcBytes[s + 0];
            dstBytes[d + 1] = srcBytes[s + 1];
            dstBytes[d + 2] = srcBytes[s + 2];
            dstBytes[d + 3] = srcBytes[s + 3];
        }
    }
    return Nothofagus::DirectTexture(std::move(dst));
}

/// Builds a headless canvas, runs the one-shot `setup`, ticks `frames` frames (calling
/// `perTick(canvas, frameIndex)` each frame for scenes that must schedule RTT/ImGui work
/// every frame), captures, then upscales and saves media/feature_<name>.png.
///
/// pixelSize is fixed at 1: the headless screenshot copies a gameSize-sized region from
/// the framebuffer origin, so any pixelSize > 1 would capture only a corner. Crisp display
/// size comes from the `upscale` nearest-neighbor factor instead.
void captureScene(
    const std::string& name,
    Nothofagus::ScreenSize size,
    glm::vec3 clearColor,
    const std::function<void(Nothofagus::Canvas&)>& setup,
    int frames = 3,
    const std::function<void(Nothofagus::Canvas&, int)>& perTick = {},
    int upscale = 4)
{
    Nothofagus::Canvas canvas(size, "README screenshot: " + name, clearColor, 1u, 14.0f, /*headless=*/true);

    setup(canvas);

    for (int frameIndex = 0; frameIndex < frames; ++frameIndex)
    {
        if (perTick)
            canvas.tick(16.0f, [&](float) { perTick(canvas, frameIndex); });
        else
            canvas.tick(16.0f);
    }

    Nothofagus::DirectTexture shot = canvas.takeScreenshot();

    // The framebuffer is cleared with alpha 0, so the captured background (and any
    // blended pixels) carry alpha < 255 even though the RGB is correct. Flatten alpha
    // to fully opaque so the PNG displays the intended colors over any page background.
    Nothofagus::TextureData data = shot.generateTextureData();
    std::span<std::uint8_t> bytes = data.getDataSpan();
    for (std::size_t i = 3; i < bytes.size(); i += 4)
        bytes[i] = 255;

    Nothofagus::DirectTexture upscaled = nearestUpscale(data, upscale);
    const std::string path = kMediaDir + "/feature_" + name + ".png";
    Nothofagus::TestHelpers::save(path, upscaled);
    spdlog::info("wrote {} ({}x{} px)", path, upscaled.size().x, upscaled.size().y);
}

// ---------------------------------------------------------------------------
// Direct textures — adapted from examples/hello_direct_texture.cpp
// ---------------------------------------------------------------------------
void directTextureScene(Nothofagus::Canvas& canvas)
{
    Nothofagus::DirectTexture texture(glm::ivec2{5, 5});
    texture.setColor(0, 0, glm::vec4(0, 0, 0, 1));
    texture.setColor(0, 1, glm::vec4(1, 1, 1, 1));
    texture.setColor(1, 1, glm::vec4(1, 0, 0, 1));
    texture.setColor(2, 1, glm::vec4(0, 1, 0, 1));
    texture.setColor(3, 1, glm::vec4(0, 0, 1, 1));
    texture.setColor(1, 2, glm::vec4(1, 1, 0, 1));
    texture.setColor(2, 2, glm::vec4(0, 1, 1, 1));
    texture.setColor(3, 2, glm::vec4(1, 0, 1, 1));
    texture.setColor(1, 3, glm::vec4(0, 0, 0, 1));
    texture.setColor(2, 3, glm::vec4(0, 0, 0, 1));
    texture.setColor(3, 3, glm::vec4(1, 1, 1, 1));
    Nothofagus::TextureId textureId = canvas.addTexture(texture);

    canvas.addBellota({{{30.0f, 80.0f}, 4}, textureId});
    canvas.addBellota({{{75.0f, 50.0f}, 8}, textureId});
    Nothofagus::BellotaId tinted = canvas.addBellota({{{50.0f, 20.0f}, 4}, textureId});
    canvas.setTint(tinted, {0.5, glm::vec3(1, 0, 0)});

    // Texture backed by externally-owned memory, wrapped via a span.
    static std::vector<std::uint8_t> externalTextureMemory{
        255, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 255, 0, 0, 255,
    };
    std::span<std::uint8_t> textureDataSpan(externalTextureMemory.begin(), externalTextureMemory.end());
    Nothofagus::DirectTexture externalTexture(textureDataSpan, {2, 2});
    Nothofagus::TextureId externalTextureId = canvas.addTexture(externalTexture);
    canvas.addBellota({{{115.0f, 80.0f}, 6}, externalTextureId});
}

// ---------------------------------------------------------------------------
// Text rendering — adapted from examples/hello_text.cpp (fixed seed for repeatable noise)
// ---------------------------------------------------------------------------
void textScene(Nothofagus::Canvas& canvas)
{
    Nothofagus::ColorPallete pallete1{
        {0.0, 0.0, 0.0, 1.0}, {1.0, 0.0, 0.0, 1.0}, {0.0, 1.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0}, {1.0, 1.0, 0.0, 1.0}, {0.0, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0}};
    pallete1 *= 0.5;
    pallete1 += glm::vec3(0.0, 0.5, 0.0);

    Nothofagus::ColorPallete pallete2{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}};
    Nothofagus::ColorPallete pallete3{{0.0, 0.0, 0.0, 0.8}, {1.0, 1.0, 1.0, 1.0}};

    Nothofagus::IndirectTexture texture1({15, 10}, {0.5, 0.5, 0.5, 1.0});
    texture1.setPallete(pallete1);
    {
        std::mt19937 rng(12345u); // fixed seed → reproducible noise across runs
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, pallete1.size() - 1);
        for (std::size_t i = 0; i < texture1.size().x; ++i)
            for (std::size_t j = 0; j < texture1.size().y; ++j)
                texture1.setPixel(i, j, Nothofagus::Pixel{static_cast<std::uint8_t>(dist(rng))});
    }
    Nothofagus::TextureId textureId1 = canvas.addTexture(texture1);

    Nothofagus::IndirectTexture texture2({8, 8}, {0.5, 0.5, 0.5, 1.0});
    texture2.setPallete(pallete2);
    Nothofagus::writeChar(texture2, 0xD, 0, 0, Nothofagus::FontType::Hiragana);
    Nothofagus::TextureId textureId2 = canvas.addTexture(texture2);

    std::string text = "- Nothofagus -";
    Nothofagus::IndirectTexture texture3({8 * text.size(), 8}, {0.5, 0.5, 0.5, 1.0});
    texture3.setPallete(pallete3);
    Nothofagus::writeText(texture3, text);
    Nothofagus::TextureId textureId3 = canvas.addTexture(texture3);

    canvas.addBellota({{{75.0f, 50.0f}, 10.0}, textureId1, -1});
    canvas.addBellota({{{75.0f, 90.0f}}, textureId2});
    canvas.addBellota({{{75.0f, 50.0f}}, textureId3});
}

// ---------------------------------------------------------------------------
// Tilemaps — adapted from examples/hello_tilemap.cpp
// ---------------------------------------------------------------------------
std::vector<std::uint8_t> makeCircleTile(glm::ivec2 tileSize)
{
    const int w = tileSize.x, h = tileSize.y;
    const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
    const float r2 = (w * 0.5f - 0.5f) * (w * 0.5f - 0.5f);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float dx = x - cx, dy = y - cy;
            data[static_cast<std::size_t>(y * w + x)] = (dx * dx + dy * dy <= r2) ? 2 : 1; // White : Black
        }
    return data;
}

std::vector<std::uint8_t> makeDitherGradientTile(glm::ivec2 tileSize)
{
    const int w = tileSize.x, h = tileSize.y;
    const float maxD = static_cast<float>((w - 1) + (h - 1));
    static constexpr float kBayer[2][2] = {{0.25f, 0.75f}, {0.75f, 0.25f}};
    std::vector<std::uint8_t> data(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float d = static_cast<float>(x + y) / maxD;
            data[static_cast<std::size_t>(y * w + x)] = (d > kBayer[y % 2][x % 2]) ? 4 : 3; // Yellow : Red
        }
    return data;
}

void tilemapScene(Nothofagus::Canvas& canvas)
{
    constexpr glm::ivec2 tileSize{8, 8};
    constexpr glm::ivec2 mapSize{4, 3};
    constexpr std::size_t tileCount = 2;

    Nothofagus::IndirectTexture tileMap(tileSize, glm::vec4(0.0f), tileCount);
    tileMap.setPallete(Nothofagus::ColorPallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}});

    auto circlePx = makeCircleTile(tileSize);
    tileMap.setPixels(std::span<const std::uint8_t>(circlePx), 0);
    auto ditherPx = makeDitherGradientTile(tileSize);
    tileMap.setPixels(std::span<const std::uint8_t>(ditherPx), 1);

    tileMap.setMap(mapSize);
    for (int row = 0; row < mapSize.y; ++row)
        for (int col = 0; col < mapSize.x; ++col)
            tileMap.setCell(col, row, static_cast<std::uint8_t>((col + row) % 2));

    Nothofagus::TextureId tileMapTexId = canvas.addTexture(tileMap);
    const glm::vec2 center{mapSize.x * tileSize.x * 0.5f, mapSize.y * tileSize.y * 0.5f};
    canvas.addBellota({Nothofagus::Transform(center), tileMapTexId});
}

// ---------------------------------------------------------------------------
// Custom meshes — adapted from examples/hello_mesh.cpp (static, no rotation/ImGui)
// ---------------------------------------------------------------------------
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;

Nothofagus::Mesh makeTriangle(float radius)
{
    Nothofagus::Mesh mesh;
    for (int i = 0; i < 3; ++i)
    {
        const float angle = i * (kTwoPi / 3.0f) + kHalfPi;
        mesh.vertices.push_back({{radius * std::cos(angle), radius * std::sin(angle)},
                                 {0.5f + 0.5f * std::cos(angle), 0.5f - 0.5f * std::sin(angle)}});
    }
    mesh.indices = {0, 1, 2};
    return mesh;
}

Nothofagus::Mesh makePentagon(float radius)
{
    Nothofagus::Mesh mesh;
    mesh.vertices.push_back({{0.0f, 0.0f}, {0.5f, 0.5f}});
    for (int i = 0; i < 5; ++i)
    {
        const float angle = i * (kTwoPi / 5.0f) + kHalfPi;
        mesh.vertices.push_back({{radius * std::cos(angle), radius * std::sin(angle)},
                                 {0.5f + 0.5f * std::cos(angle), 0.5f - 0.5f * std::sin(angle)}});
    }
    for (unsigned int i = 0; i < 5; ++i)
    {
        mesh.indices.push_back(0);
        mesh.indices.push_back(1 + i);
        mesh.indices.push_back(1 + ((i + 1) % 5));
    }
    return mesh;
}

Nothofagus::Mesh makeQuad(float halfSide)
{
    Nothofagus::Mesh mesh;
    mesh.vertices = {
        {{-halfSide, -halfSide}, {0.0f, 1.0f}}, {{halfSide, -halfSide}, {1.0f, 1.0f}},
        {{halfSide, halfSide}, {1.0f, 0.0f}}, {{-halfSide, halfSide}, {0.0f, 0.0f}}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

void meshScene(Nothofagus::Canvas& canvas)
{
    Nothofagus::ColorPallete pallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.7f, 0.2f, 1.0f},
        {0.2f, 0.8f, 1.0f, 1.0f}, {0.9f, 0.3f, 0.4f, 1.0f}};
    Nothofagus::IndirectTexture texture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    texture.setPallete(pallete).setPixels({
        1, 1, 2, 2, 2, 2, 1, 1, 1, 2, 2, 3, 3, 2, 2, 1,
        2, 2, 3, 3, 3, 3, 2, 2, 2, 3, 3, 1, 1, 3, 3, 2,
        2, 3, 3, 1, 1, 3, 3, 2, 2, 2, 3, 3, 3, 3, 2, 2,
        1, 2, 2, 3, 3, 2, 2, 1, 1, 1, 2, 2, 2, 2, 1, 1});
    const Nothofagus::TextureId textureId = canvas.addTexture(texture);

    const Nothofagus::MeshId triangleMeshId = canvas.addMesh(makeTriangle(20.0f));
    const Nothofagus::MeshId pentagonMeshId = canvas.addMesh(makePentagon(20.0f));
    const Nothofagus::MeshId quadMeshId = canvas.addMesh(makeQuad(18.0f));

    canvas.addBellota({{{60.0f, 75.0f}}, textureId, triangleMeshId, std::int8_t{1}});
    canvas.addBellota({{{140.0f, 75.0f}}, textureId, pentagonMeshId});
    canvas.addBellota({{{100.0f, 25.0f}}, textureId, quadMeshId});
}

// ---------------------------------------------------------------------------
// Screenshots — adapted from examples/hello_screenshot.cpp (picture-in-picture)
// ---------------------------------------------------------------------------
void addScreenshotThumbnail(Nothofagus::Canvas& canvas)
{
    constexpr int borderSize = 5;
    constexpr float thumbnailScale = 0.22f;

    Nothofagus::DirectTexture shot = canvas.takeScreenshot();
    const Nothofagus::ScreenSize sz = canvas.screenSize();
    const int sw = static_cast<int>(sz.width), sh = static_cast<int>(sz.height);

    // White ring frame with a transparent interior, sized to the screenshot + border.
    const int bw = sw + borderSize * 2, bh = sh + borderSize * 2;
    Nothofagus::TextureData frameData(bw, bh, 1);
    auto frameSpan = frameData.getDataSpan();
    std::fill(frameSpan.begin(), frameSpan.end(), std::uint8_t{255});
    for (int row = borderSize; row < bh - borderSize; ++row)
    {
        std::uint8_t* interiorRow = frameSpan.data() + (row * bw + borderSize) * 4;
        std::fill(interiorRow, interiorRow + (bw - borderSize * 2) * 4, std::uint8_t{0});
    }
    const float thumbHalfW = bw * thumbnailScale * 0.5f;
    const float thumbHalfH = bh * thumbnailScale * 0.5f;
    const glm::vec2 thumbPos{static_cast<float>(sw) - thumbHalfW - 2.0f, thumbHalfH + 2.0f};

    Nothofagus::TextureId frameTexId = canvas.addTexture(Nothofagus::DirectTexture(std::move(frameData)));
    canvas.addBellota({{thumbPos, thumbnailScale}, frameTexId, std::int8_t{2}});

    Nothofagus::TextureId shotTexId = canvas.addTexture(shot);
    canvas.addBellota({{thumbPos, thumbnailScale}, shotTexId, std::int8_t{3}});
}

int run()
{
    std::filesystem::create_directories(kMediaDir);

    captureScene("direct_texture", {150, 100}, {0.7f, 0.7f, 0.7f}, directTextureScene, 3, {}, 4);
    captureScene("text", {150, 100}, {0.7f, 0.7f, 0.7f}, textScene, 3, {}, 4);
    captureScene("tilemap", {32, 24}, {0.0f, 0.0f, 0.0f}, tilemapScene, 3, {}, 16);
    captureScene("mesh", {200, 150}, {0.1f, 0.1f, 0.15f}, meshScene, 3, {}, 3);

    // --- Render to texture (perTick must re-schedule the RT pass each frame) ---
    {
        Nothofagus::RenderTargetId rtId;
        std::vector<Nothofagus::BellotaId> sources;
        captureScene(
            "render_to_texture", {128, 128}, {0.15f, 0.15f, 0.15f},
            [&](Nothofagus::Canvas& canvas) {
                Nothofagus::ColorPallete redPallete{
                    {0.0f, 0.0f, 0.0f, 0.0f}, {0.8f, 0.1f, 0.1f, 1.0f}, {1.0f, 0.5f, 0.5f, 1.0f}};
                Nothofagus::IndirectTexture redTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
                redTexture.setPallete(redPallete).setPixels({
                    0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 2, 2, 2, 2, 1, 0,
                    1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 1, 2, 2, 1, 2, 1,
                    1, 2, 1, 2, 2, 1, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1,
                    0, 1, 2, 2, 2, 2, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0});
                Nothofagus::TextureId redTextureId = canvas.addTexture(redTexture);

                Nothofagus::ColorPallete bluePallete{
                    {0.0f, 0.0f, 0.0f, 0.0f}, {0.1f, 0.1f, 0.8f, 1.0f}, {0.5f, 0.5f, 1.0f, 1.0f}};
                Nothofagus::IndirectTexture blueTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
                blueTexture.setPallete(bluePallete).setPixels({
                    0, 1, 1, 1, 1, 1, 1, 0, 1, 2, 2, 2, 2, 2, 2, 1,
                    1, 2, 0, 0, 0, 0, 2, 1, 1, 2, 0, 1, 1, 0, 2, 1,
                    1, 2, 0, 1, 1, 0, 2, 1, 1, 2, 0, 0, 0, 0, 2, 1,
                    1, 2, 2, 2, 2, 2, 2, 1, 0, 1, 1, 1, 1, 1, 1, 0});
                Nothofagus::TextureId blueTextureId = canvas.addTexture(blueTexture);

                Nothofagus::BellotaId redBellotaId = canvas.addBellota({{{22.0f, 32.0f}}, redTextureId});
                Nothofagus::BellotaId blueBellotaId = canvas.addBellota({{{42.0f, 32.0f}}, blueTextureId});
                canvas.bellota(redBellotaId).transform().angle() = 25.0f;
                canvas.bellota(blueBellotaId).transform().angle() = -25.0f;
                canvas.addBellota({{{100.0f, 32.0f}}, blueTextureId, std::int8_t{1}});

                rtId = canvas.addRenderTarget({64, 64});
                canvas.setRenderTargetClearColor(rtId, {0.0f, 0.0f, 0.0f, 0.5f});
                Nothofagus::TextureId rtTexId = canvas.renderTargetTexture(rtId);
                canvas.addBellota({{{64.0f, 72.0f}}, rtTexId});

                sources = {redBellotaId, blueBellotaId};
            },
            6,
            [&](Nothofagus::Canvas& canvas, int) { canvas.renderTo(rtId, sources); },
            4);
    }

    // --- Nested render targets (schedule innermost-first each frame) ---
    {
        Nothofagus::RenderTargetId innerId, middleId;
        std::vector<Nothofagus::BellotaId> innerSources, middleSources;
        captureScene(
            "nested_render_targets", {128, 128}, {0.05f, 0.05f, 0.05f},
            [&](Nothofagus::Canvas& canvas) {
                auto makeSprite = [&](glm::vec3 c1, glm::vec3 c2) {
                    Nothofagus::ColorPallete pal{
                        {0.0f, 0.0f, 0.0f, 0.0f}, {c1.r, c1.g, c1.b, 1.0f}, {c2.r, c2.g, c2.b, 1.0f}};
                    Nothofagus::IndirectTexture t({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
                    t.setPallete(pal).setPixels({
                        0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0,
                        0, 1, 2, 2, 2, 2, 1, 0, 1, 2, 2, 2, 2, 2, 2, 1,
                        1, 2, 2, 2, 2, 2, 2, 1, 0, 1, 2, 2, 2, 2, 1, 0,
                        0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0});
                    return canvas.addTexture(t);
                };
                Nothofagus::TextureId redTextureId = makeSprite({0.8f, 0.1f, 0.1f}, {1.0f, 0.5f, 0.3f});
                Nothofagus::TextureId yellowTextureId = makeSprite({0.9f, 0.8f, 0.0f}, {1.0f, 1.0f, 0.5f});
                Nothofagus::TextureId blueTextureId = makeSprite({0.1f, 0.2f, 0.9f}, {0.4f, 0.6f, 1.0f});

                // Level 1 (32x32): red + yellow placed apart for a clear still.
                Nothofagus::BellotaId redBellotaId = canvas.addBellota({{{23.0f, 21.0f}}, redTextureId});
                Nothofagus::BellotaId yellowBellotaId = canvas.addBellota({{{9.0f, 11.0f}}, yellowTextureId});
                innerId = canvas.addRenderTarget({32, 32});
                canvas.setRenderTargetClearColor(innerId, {0.05f, 0.02f, 0.02f, 1.0f});
                Nothofagus::TextureId innerTexId = canvas.renderTargetTexture(innerId);

                // Level 2 (64x64): inner display + blue.
                Nothofagus::BellotaId innerDisplayId = canvas.addBellota({{{20.0f, 32.0f}}, innerTexId});
                Nothofagus::BellotaId blueBellotaId = canvas.addBellota({{{48.0f, 38.0f}}, blueTextureId});
                canvas.bellota(innerDisplayId).transform().angle() = 12.0f;
                middleId = canvas.addRenderTarget({64, 64});
                canvas.setRenderTargetClearColor(middleId, {0.02f, 0.02f, 0.08f, 1.0f});
                Nothofagus::TextureId middleTexId = canvas.renderTargetTexture(middleId);

                // Level 3 (main canvas): middle display, slightly rotated.
                Nothofagus::BellotaId middleDisplayId = canvas.addBellota({{{64.0f, 64.0f}}, middleTexId});
                canvas.bellota(middleDisplayId).transform().angle() = 6.0f;

                innerSources = {redBellotaId, yellowBellotaId};
                middleSources = {innerDisplayId, blueBellotaId};
            },
            6,
            [&](Nothofagus::Canvas& canvas, int) {
                canvas.renderTo(innerId, innerSources);
                canvas.renderTo(middleId, middleSources);
            },
            4);
    }

    // NOTE: the "Diegetic ImGui + custom fonts" feature (renderImguiTo into an RTT) is
    // intentionally not captured here. In the headless-Vulkan backend that path corrupts
    // the whole frame (the secondary-context ImGui RTT pass is untested headless), so it
    // produces a blank capture. That README subsection stays text-only for now.

    // --- Markdown rendering (MarkdownRenderer must outlive the ticks) ---
    {
        std::optional<Nothofagus::MarkdownRenderer> markdown;
        static constexpr const char* kSample = R"md(# Markdown rendering

**Nothofagus** renders *emphasis*, **strong**, ***both***,
and `inline code` as distinct faces.

## Lists
- Apples
- Oranges
- Bananas

```cpp
markdown.print("# Heading");
```

> Blockquotes and ~~strikethrough~~ too.
)md";
        captureScene(
            "markdown", {320, 240}, {0.10f, 0.10f, 0.14f},
            [&](Nothofagus::Canvas& canvas) {
                markdown.emplace(canvas);
                markdown->setStyle(canvas.defaultMarkdownStyle(13.0f));
            },
            6,
            [&](Nothofagus::Canvas& canvas, int) {
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
                ImGui::SetNextWindowSize(
                    ImVec2(static_cast<float>(canvas.screenSize().width),
                           static_cast<float>(canvas.screenSize().height)),
                    ImGuiCond_Always);
                ImGui::Begin("docs", nullptr,
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
                markdown->print(kSample);
                ImGui::End();
            },
            2);
    }

    // --- Screenshots (capture the scene, then add it back as a framed thumbnail) ---
    captureScene(
        "screenshots", {128, 96}, {0.1f, 0.15f, 0.2f},
        [&](Nothofagus::Canvas& canvas) {
            Nothofagus::IndirectTexture sprite({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
            sprite.setPallete({
                {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.3f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.3f, 1.0f},
                {0.2f, 0.4f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.2f, 1.0f}});
            sprite.setPixels({
                0, 0, 1, 1, 2, 2, 0, 0, 0, 1, 1, 2, 2, 3, 3, 0,
                1, 1, 4, 4, 4, 4, 3, 3, 1, 4, 4, 1, 2, 4, 4, 3,
                2, 4, 4, 2, 1, 4, 4, 3, 2, 2, 4, 4, 4, 4, 3, 3,
                0, 2, 2, 3, 3, 3, 3, 0, 0, 0, 2, 2, 3, 3, 0, 0});
            Nothofagus::TextureId spriteTexId = canvas.addTexture(sprite);
            canvas.addBellota({{{64.0f, 48.0f}, 4.0f}, spriteTexId});
        },
        5,
        [&](Nothofagus::Canvas& canvas, int frameIndex) {
            // On a mid-run frame the front buffer already holds the sprite — capture
            // it and pin it back as a bordered picture-in-picture thumbnail.
            if (frameIndex == 2)
                addScreenshotThumbnail(canvas);
        },
        4);

    spdlog::info("README feature screenshots written to {}", kMediaDir);
    return 0;
}
} // namespace

int main()
{
    return run();
}
