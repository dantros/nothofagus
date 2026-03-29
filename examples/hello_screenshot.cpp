#include <nothofagus.h>
#include <optional>

int main()
{
    Nothofagus::Canvas canvas({128, 96}, "Hello Screenshot!", {0.1f, 0.15f, 0.2f}, 6);

    // A small colorful sprite to give the scene some content.
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
    auto spriteId    = canvas.addBellota({{{64.f, 48.f}}, spriteTexId});

    // Holds the bellota displaying the last captured screenshot thumbnail.
    std::optional<Nothofagus::BellotaId> screenshotBellotaId;

    float time = 0.0f;
    auto update = [&](float dt)
    {
        time += dt;
        canvas.bellota(spriteId).transform().angle() = 0.05f * time;

        ImGui::SetNextWindowPos({4, 4});
        ImGui::Begin("Controls", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("SPACE: capture screenshot");
        ImGui::End();
    };

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::SPACE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        // Remove the previous thumbnail so its texture is garbage-collected.
        if (screenshotBellotaId)
            canvas.removeBellota(*screenshotBellotaId);

        // Capture the current frame as a DirectTexture.
        // The returned object owns its RGBA pixel data and can be passed to
        // stb_image_plus (or any other image library) for saving to disk:
        //
        //   auto data = screenshot.generateTextureData();
        //   auto span = data.getDataSpan();
        //   stb_image_plus::ImageData4 img(
        //       {reinterpret_cast<stb_image_plus::Pixel4*>(span.data()), span.size() / 4},
        //       data.width(), data.height());
        //   img.write("screenshot.png");
        Nothofagus::DirectTexture screenshot = canvas.takeScreenshot();
        spdlog::info("Screenshot captured: {}x{} px", screenshot.size().x, screenshot.size().y);

        // Display the screenshot as a scaled-down thumbnail in the bottom-left corner.
        auto screenshotTexId = canvas.addTexture(screenshot);
        screenshotBellotaId  = canvas.addBellota({{{16.f, 16.f}, 0.25f}, screenshotTexId});
    });

    canvas.run(update, controller);
    return 0;
}
