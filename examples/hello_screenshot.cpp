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

    // Screenshot thumbnail constants.
    constexpr int   borderSize                  = 5;
    constexpr float thumbnailScale              = 0.2f;
    constexpr float screenshotDisplayDurationMs = 2000.0f;

    // Screenshot thumbnail state.
    std::optional<Nothofagus::BellotaId> screenshotBellotaId;
    std::optional<Nothofagus::BellotaId> screenshotFrameBellotaId;
    float                  screenshotTimer    = 0.0f;
    Nothofagus::ScreenSize frameBuiltForSize  = {0, 0};
    glm::vec2              thumbnailPos       = {0.0f, 0.0f};

    // Builds (or rebuilds) the persistent frame bellota for the current canvas resolution.
    // The frame is a white ring with a transparent interior. It is kept hidden (opacity 0)
    // until a screenshot is captured and shown again (opacity 1) while the thumbnail is displayed.
    // If the canvas resolution changed since the last build, the old bellota is replaced.
    auto ensureFrame = [&]()
    {
        const Nothofagus::ScreenSize currentSize = canvas.screenSize();
        if (screenshotFrameBellotaId &&
            frameBuiltForSize.width  == currentSize.width &&
            frameBuiltForSize.height == currentSize.height)
            return;

        if (screenshotFrameBellotaId)
        {
            canvas.removeBellota(*screenshotFrameBellotaId);
            screenshotFrameBellotaId.reset();
        }

        const int sw = static_cast<int>(currentSize.width);
        const int sh = static_cast<int>(currentSize.height);
        const int bw = sw + borderSize * 2;
        const int bh = sh + borderSize * 2;

        Nothofagus::TextureData frameData(bw, bh, 1);
        auto frameSpan = frameData.getDataSpan();
        std::fill(frameSpan.begin(), frameSpan.end(), std::uint8_t{255});
        for (int row = borderSize; row < bh - borderSize; ++row)
        {
            std::uint8_t* interiorRow = frameSpan.data() + row * bw * 4 + borderSize * 4;
            std::fill(interiorRow, interiorRow + (bw - borderSize * 2) * 4, std::uint8_t{0});
        }

        const float thumbnailHalfW = bw * thumbnailScale / 2.0f;
        const float thumbnailHalfH = bh * thumbnailScale / 2.0f;
        thumbnailPos = {static_cast<float>(sw) - thumbnailHalfW - 2.0f, thumbnailHalfH + 2.0f};

        auto frameTexId          = canvas.addTexture(Nothofagus::DirectTexture(std::move(frameData)));
        screenshotFrameBellotaId = canvas.addBellota({{thumbnailPos, thumbnailScale}, frameTexId, 2});
        canvas.bellota(*screenshotFrameBellotaId).opacity() = 0.0f;
        frameBuiltForSize = currentSize;
    };

    auto update = [&](float dt)
    {
        // Pick up a scheduled screenshot once it is ready (the frame after SPACE was
        // pressed). The returned DirectTexture owns its RGBA pixels (top-to-bottom) and
        // can be saved with an external image library, e.g.:
        //   auto data = shot->generateTextureData();
        //   auto span = data.getDataSpan();
        //   stb_image_plus::ImageData4 img(
        //       {reinterpret_cast<stb_image_plus::Pixel4*>(span.data()), span.size() / 4},
        //       data.width(), data.height());
        //   img.write("screenshot.png");
        if (std::optional<Nothofagus::DirectTexture> shot = canvas.retrieveScreenshot())
        {
            // Remove any existing screenshot content so its texture is garbage-collected.
            if (screenshotBellotaId)
            {
                canvas.removeBellota(*screenshotBellotaId);
                screenshotBellotaId.reset();
            }
            ensureFrame(); // persistent frame bellota, matched to the current resolution

            spdlog::info("Screenshot captured: {}x{} px", shot->size().x, shot->size().y);
            auto screenshotTexId = canvas.addTexture(*shot);
            screenshotBellotaId  = canvas.addBellota({{thumbnailPos, thumbnailScale}, screenshotTexId, 1});

            canvas.bellota(*screenshotFrameBellotaId).opacity() = 1.0f;
            screenshotTimer = screenshotDisplayDurationMs;
        }

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
                canvas.bellota(*screenshotFrameBellotaId).opacity() = 0.0f;
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
        // Schedule a screenshot of the next rendered frame. Capture is deferred:
        // the pixels arrive via canvas.retrieveScreenshot() on the following frame (see
        // the poll at the top of update()), so it can be requested from inside a callback.
        canvas.requestScreenshot();
    });

    // TODO(threaded): scheduled screenshots cross the sim/render boundary unsynchronized —
    // requestScreenshot() arms flags on the sim thread while finishScreenshot() writes
    // mScreenshotResult on the render thread, with no marshaling. Parked on the deprecated
    // single-thread run(update, Controller&) until a threaded request/result handoff lands.
    canvas.run(update, controller);
    return 0;
}
