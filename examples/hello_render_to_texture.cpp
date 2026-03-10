#include <nothofagus.h>
#include <cmath>

int main()
{
    // Canvas: 128x128 logical pixels, 6px scale.
    Nothofagus::Canvas canvas({128, 128}, "Hello Render To Texture", {0.15f, 0.15f, 0.15f}, 6);

    // --- Source sprites (rendered into the render target each frame) ---

    Nothofagus::ColorPallete redPallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, // 0: transparent
        {0.8f, 0.1f, 0.1f, 1.0f}, // 1: red
        {1.0f, 0.5f, 0.5f, 1.0f}, // 2: light red
    };
    Nothofagus::IndirectTexture redTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    redTexture.setPallete(redPallete).setPixels({
        0,0,1,1,1,1,0,0,
        0,1,2,2,2,2,1,0,
        1,2,2,1,1,2,2,1,
        1,2,1,2,2,1,2,1,
        1,2,1,2,2,1,2,1,
        1,2,2,1,1,2,2,1,
        0,1,2,2,2,2,1,0,
        0,0,1,1,1,1,0,0,
    });
    Nothofagus::TextureId redTextureId = canvas.addTexture(redTexture);

    Nothofagus::ColorPallete bluePallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, // 0: transparent
        {0.1f, 0.1f, 0.8f, 1.0f}, // 1: blue
        {0.5f, 0.5f, 1.0f, 1.0f}, // 2: light blue
    };
    Nothofagus::IndirectTexture blueTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    blueTexture.setPallete(bluePallete).setPixels({
        0,1,1,1,1,1,1,0,
        1,2,2,2,2,2,2,1,
        1,2,0,0,0,0,2,1,
        1,2,0,1,1,0,2,1,
        1,2,0,1,1,0,2,1,
        1,2,0,0,0,0,2,1,
        1,2,2,2,2,2,2,1,
        0,1,1,1,1,1,1,0,
    });
    Nothofagus::TextureId blueTextureId = canvas.addTexture(blueTexture);

    // Source bellotas — positioned inside the 64x64 render target coordinate space.
    Nothofagus::BellotaId redBellotaId  = canvas.addBellota({{{22.0f, 32.0f}}, redTextureId});
    Nothofagus::BellotaId blueBellotaId = canvas.addBellota({{{42.0f, 32.0f}}, blueTextureId});

    // --- Render target: 64x64 pixels ---

    Nothofagus::RenderTargetId renderTargetId = canvas.addRenderTarget({64, 64});
    Nothofagus::TextureId renderTargetTextureId = canvas.renderTargetTexture(renderTargetId);
    canvas.setRenderTargetClearColor(renderTargetId, {0.0f, 0.0f, 0.0f, 1.0f});

    // Display bellota — samples the render target and shows it on the main canvas.
    Nothofagus::BellotaId displayBellotaId = canvas.addBellota({{{64.0f, 64.0f}}, renderTargetTextureId});

    float time = 0.0f;

    canvas.run([&](float deltaTime)
    {
        time += deltaTime;

        // Rotate source sprites over time.
        canvas.bellota(redBellotaId).transform().angle()  =  0.05f * time;
        canvas.bellota(blueBellotaId).transform().angle() = -0.05f * time;

        // Gently bob the display bellota.
        canvas.bellota(displayBellotaId).transform().location().y = 96.0f + 8.0f * std::sin(0.002f * time);

        // Schedule both source sprites to be rendered into the render target this frame.
        canvas.renderTo(renderTargetId, {redBellotaId, blueBellotaId});
    });

    return 0;
}
