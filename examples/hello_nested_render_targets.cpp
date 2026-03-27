#include <nothofagus.h>
#include <cmath>
#include <numbers>

int main()
{
    // Main canvas: 128x128 logical pixels, 5px scale.
    Nothofagus::Canvas canvas({128, 128}, "Nested Render Targets", {0.05f, 0.05f, 0.05f}, 5);

    // -------------------------------------------------------------------------
    // Sprites
    // -------------------------------------------------------------------------

    Nothofagus::ColorPallete redPallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, // 0: transparent
        {0.8f, 0.1f, 0.1f, 1.0f}, // 1: red
        {1.0f, 0.5f, 0.3f, 1.0f}, // 2: light red
    };
    Nothofagus::IndirectTexture redTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    redTexture.setPallete(redPallete).setPixels({
        0,0,0,1,1,0,0,0,
        0,0,1,2,2,1,0,0,
        0,1,2,2,2,2,1,0,
        1,2,2,1,1,2,2,1,
        1,2,2,1,1,2,2,1,
        0,1,2,2,2,2,1,0,
        0,0,1,2,2,1,0,0,
        0,0,0,1,1,0,0,0,
    });
    Nothofagus::TextureId redTextureId = canvas.addTexture(redTexture);

    Nothofagus::ColorPallete yellowPallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, // 0: transparent
        {0.9f, 0.8f, 0.0f, 1.0f}, // 1: yellow
        {1.0f, 1.0f, 0.5f, 1.0f}, // 2: light yellow
    };
    Nothofagus::IndirectTexture yellowTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    yellowTexture.setPallete(yellowPallete).setPixels({
        0,0,1,1,1,1,0,0,
        0,1,2,2,2,2,1,0,
        1,2,2,0,0,2,2,1,
        1,2,0,2,2,0,2,1,
        1,2,0,2,2,0,2,1,
        1,2,2,0,0,2,2,1,
        0,1,2,2,2,2,1,0,
        0,0,1,1,1,1,0,0,
    });
    Nothofagus::TextureId yellowTextureId = canvas.addTexture(yellowTexture);

    Nothofagus::ColorPallete bluePallete{
        {0.0f, 0.0f, 0.0f, 0.0f}, // 0: transparent
        {0.1f, 0.2f, 0.9f, 1.0f}, // 1: blue
        {0.4f, 0.6f, 1.0f, 1.0f}, // 2: light blue
    };
    Nothofagus::IndirectTexture blueTexture({8, 8}, {0.0f, 0.0f, 0.0f, 0.0f});
    blueTexture.setPallete(bluePallete).setPixels({
        0,0,0,1,1,0,0,0,
        0,0,1,2,2,1,0,0,
        0,1,2,2,2,2,1,0,
        1,2,2,2,2,2,2,1,
        1,2,2,2,2,2,2,1,
        0,1,2,2,2,2,1,0,
        0,0,1,2,2,1,0,0,
        0,0,0,1,1,0,0,0,
    });
    Nothofagus::TextureId blueTextureId = canvas.addTexture(blueTexture);

    // -------------------------------------------------------------------------
    // Level 1 — inner render target (32x32)
    // Red and yellow sprites orbit each other inside this small target.
    // -------------------------------------------------------------------------

    Nothofagus::BellotaId redBellotaId    = canvas.addBellota({{{16.0f, 16.0f}}, redTextureId});
    Nothofagus::BellotaId yellowBellotaId = canvas.addBellota({{{16.0f, 16.0f}}, yellowTextureId});

    Nothofagus::RenderTargetId innerRenderTargetId = canvas.addRenderTarget({32, 32});
    canvas.setRenderTargetClearColor(innerRenderTargetId, {0.05f, 0.02f, 0.02f, 1.0f});
    Nothofagus::TextureId innerRenderTargetTextureId = canvas.renderTargetTexture(innerRenderTargetId);

    // -------------------------------------------------------------------------
    // Level 2 — middle render target (64x64)
    // The inner display bellota (32x32) and a blue sprite are rendered here.
    // Both live in the middle RT's coordinate space (0..64).
    // -------------------------------------------------------------------------

    Nothofagus::BellotaId innerDisplayBellotaId = canvas.addBellota({{{20.0f, 32.0f}}, innerRenderTargetTextureId});
    Nothofagus::BellotaId blueBellotaId         = canvas.addBellota({{{46.0f, 32.0f}}, blueTextureId});

    Nothofagus::RenderTargetId middleRenderTargetId = canvas.addRenderTarget({64, 64});
    canvas.setRenderTargetClearColor(middleRenderTargetId, {0.02f, 0.02f, 0.08f, 1.0f});
    Nothofagus::TextureId middleRenderTargetTextureId = canvas.renderTargetTexture(middleRenderTargetId);

    // -------------------------------------------------------------------------
    // Level 3 — main canvas (128x128)
    // The middle display bellota (64x64) is shown here, slowly rotating.
    // -------------------------------------------------------------------------

    Nothofagus::BellotaId middleDisplayBellotaId = canvas.addBellota({{{64.0f, 64.0f}}, middleRenderTargetTextureId});

    float time = 0.0f;
    constexpr float pi = std::numbers::pi_v<float>;

    canvas.run([&](float deltaTime)
    {
        time += deltaTime;

        // Level 1: red and yellow orbit around the center of the 32x32 inner RT.
        constexpr float innerOrbitRadius = 9.0f;
        canvas.bellota(redBellotaId).transform().location() = {
            16.0f + innerOrbitRadius * std::cos( 0.003f * time),
            16.0f + innerOrbitRadius * std::sin( 0.003f * time),
        };
        canvas.bellota(yellowBellotaId).transform().location() = {
            16.0f + innerOrbitRadius * std::cos( 0.003f * time + pi),
            16.0f + innerOrbitRadius * std::sin( 0.003f * time + pi),
        };

        // Level 2: inner display bellota gently rotates; blue sprite orbits in 64x64 space.
        canvas.bellota(innerDisplayBellotaId).transform().angle() = 0.04f * time;

        constexpr float middleOrbitRadius = 14.0f;
        canvas.bellota(blueBellotaId).transform().location() = {
            46.0f + middleOrbitRadius * std::cos(-0.002f * time),
            32.0f + middleOrbitRadius * std::sin(-0.002f * time),
        };

        // Level 3: middle display bellota slowly rotates on the main canvas.
        canvas.bellota(middleDisplayBellotaId).transform().angle() = 0.01f * time;

        // RTT passes must be scheduled innermost-first: each level is fully rendered
        // before the next level samples it as a texture.
        canvas.renderTo(innerRenderTargetId, {redBellotaId, yellowBellotaId});
        canvas.renderTo(middleRenderTargetId, {innerDisplayBellotaId, blueBellotaId});
    });

    return 0;
}
