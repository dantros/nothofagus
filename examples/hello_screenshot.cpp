#include <nothofagus.h>
#include <algorithm>
#include <optional>
#include <cmath>

int main()
{
    Nothofagus::Canvas canvas({128, 96}, "Hello Screenshot!", {0.1f, 0.15f, 0.2f}, 6);

    // Colorful 8x8 sprite displayed at 4x scale (32x32 screen pixels).
    Nothofagus::IndirectTexture sprite({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    sprite.setPallete({
        {0.0f, 0.0f, 0.0f, 0.0f},  // 0 transparent
        {1.0f, 0.3f, 0.2f, 1.0f},  // 1 red
        {0.2f, 1.0f, 0.3f, 1.0f},  // 2 green
        {0.2f, 0.4f, 1.0f, 1.0f},  // 3 blue
        {1.0f, 1.0f, 0.2f, 1.0f},  // 4 yellow
    });
    sprite.setPixels({
        0, 0, 1, 1, 2, 2, 0, 0,
        0, 1, 1, 2, 2, 3, 3, 0,
        1, 1, 4, 4, 4, 4, 3, 3,
        1, 4, 4, 1, 2, 4, 4, 3,
        2, 4, 4, 2, 1, 4, 4, 3,
        2, 2, 4, 4, 4, 4, 3, 3,
        0, 2, 2, 3, 3, 3, 3, 0,
        0, 0, 2, 2, 3, 3, 0, 0,
    });
    auto spriteTexId = canvas.addTexture(sprite);

    constexpr float spriteScale    = 4.0f;
    constexpr float spriteHalfSize = 8.0f * spriteScale / 2.0f; // 16 px

    glm::vec2 spritePos = {64.0f, 48.0f};
    glm::vec2 spriteVel = {0.07f, 0.05f}; // pixels per ms

    auto spriteId = canvas.addBellota({{spritePos, spriteScale}, spriteTexId});

    // Screenshot thumbnail state.
    std::optional<Nothofagus::BellotaId> screenshotBellotaId;
    float screenshotTimer = 0.0f;
    constexpr float screenshotDisplayDurationMs = 2000.0f;

    auto update = [&](float dt)
    {
        // Bounce the sprite inside the canvas bounds.
        spritePos += spriteVel * dt;

        if (spritePos.x - spriteHalfSize < 0.0f)
            { spritePos.x = spriteHalfSize;          spriteVel.x =  std::abs(spriteVel.x); }
        if (spritePos.x + spriteHalfSize > 128.0f)
            { spritePos.x = 128.0f - spriteHalfSize; spriteVel.x = -std::abs(spriteVel.x); }
        if (spritePos.y - spriteHalfSize < 0.0f)
            { spritePos.y = spriteHalfSize;           spriteVel.y =  std::abs(spriteVel.y); }
        if (spritePos.y + spriteHalfSize > 96.0f)
            { spritePos.y = 96.0f - spriteHalfSize;  spriteVel.y = -std::abs(spriteVel.y); }

        canvas.bellota(spriteId).transform().location() = spritePos;

        // Count down and remove the screenshot thumbnail after the display duration.
        if (screenshotBellotaId)
        {
            screenshotTimer -= dt;
            if (screenshotTimer <= 0.0f)
            {
                canvas.removeBellota(*screenshotBellotaId);
                screenshotBellotaId.reset();
            }
        }

        ImGui::SetNextWindowPos({4, 4});
        ImGui::Begin("Controls", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("SPACE: capture screenshot");
        ImGui::End();
    };

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::SPACE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        // Remove any existing thumbnail so its texture is garbage-collected.
        if (screenshotBellotaId)
        {
            canvas.removeBellota(*screenshotBellotaId);
            screenshotBellotaId.reset();
        }

        // Capture the current frame at game resolution (canvas.screenSize()).
        // The returned DirectTexture owns its RGBA pixel data (top-to-bottom row order)
        // and can be passed to an external image library for saving to disk, e.g.:
        //
        //   auto data = screenshot.generateTextureData();
        //   auto span = data.getDataSpan();
        //   stb_image_plus::ImageData4 img(
        //       {reinterpret_cast<stb_image_plus::Pixel4*>(span.data()), span.size() / 4},
        //       data.width(), data.height());
        //   img.write("screenshot.png");
        Nothofagus::DirectTexture screenshot = canvas.takeScreenshot();
        const int sw = screenshot.size().x;
        const int sh = screenshot.size().y;
        spdlog::info("Screenshot captured: {}x{} px", sw, sh);

        // Build a bordered version.
        // The thumbnail is displayed at 20% of the canvas width (scale ~0.2).
        // At scale 0.2 one screen pixel = 5 texture pixels, so a 5-pixel white
        // border appears as a clean 1-pixel outline on screen.
        constexpr int   borderSize     = 5;
        constexpr float thumbnailScale = 0.2f;

        const int bw = sw + borderSize * 2;
        const int bh = sh + borderSize * 2;

        auto screenshotData = screenshot.generateTextureData();
        auto screenshotSpan = screenshotData.getDataSpan();

        Nothofagus::TextureData borderedData(bw, bh, 1);
        auto borderedSpan = borderedData.getDataSpan();

        // Fill with opaque white, then overwrite the interior with screenshot pixels.
        std::fill(borderedSpan.begin(), borderedSpan.end(), std::uint8_t{255});
        for (int row = 0; row < sh; ++row)
        {
            const std::uint8_t* srcRow = screenshotSpan.data() + row * sw * 4;
            std::uint8_t*       dstRow = borderedSpan.data() + (row + borderSize) * bw * 4 + borderSize * 4;
            std::copy(srcRow, srcRow + sw * 4, dstRow);
        }

        // Display in the bottom-right corner, rendered on top of the scene.
        const float     thumbnailHalfW = bw * thumbnailScale / 2.0f;
        const float     thumbnailHalfH = bh * thumbnailScale / 2.0f;
        const glm::vec2 thumbnailPos   = {128.0f - thumbnailHalfW - 2.0f, thumbnailHalfH + 2.0f};

        auto screenshotTexId = canvas.addTexture(Nothofagus::DirectTexture(std::move(borderedData)));
        screenshotBellotaId  = canvas.addBellota({{thumbnailPos, thumbnailScale}, screenshotTexId, 1});
        screenshotTimer      = screenshotDisplayDurationMs;
    });

    canvas.run(update, controller);
    return 0;
}
