#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <random>
#include <variant>
#include <nothofagus.h>

int main()
{
    Nothofagus::Canvas canvas({150, 100}, "Hello Text", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete pallete1{
        {0.0, 0.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0},
        {1.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0}
    };
    pallete1 *= 0.5;
    pallete1 += glm::vec3(0.0,0.5,0.0);

    Nothofagus::ColorPallete pallete2{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };

    Nothofagus::ColorPallete pallete3{
        {0.0, 0.0, 0.0, 0.8},
        {1.0, 1.0, 1.0, 1.0 }
    };

    Nothofagus::IndirectTexture texture1({ 15, 10 }, { 0.5, 0.5, 0.5, 1.0 });
    texture1.setPallete(pallete1);
    {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, pallete1.size()-1);

        for (std::size_t i = 0; i < texture1.size().x; ++i)
        {
            for (std::size_t j = 0; j < texture1.size().y; ++j)
            {
                const Nothofagus::Pixel randomColor{ static_cast<std::uint8_t>(dist(rng)) };
                texture1.setPixel(i, j, randomColor);
            }
        }
    }

    Nothofagus::TextureId textureId1 = canvas.addTexture(texture1);

    // writeChar remains the per-glyph primitive: one glyph painted into an 8x8 texture.
    Nothofagus::IndirectTexture texture2({ 8, 8 }, { 0.5, 0.5, 0.5, 1.0 });
    texture2.setPallete(pallete2);
    Nothofagus::writeChar(texture2, 0xD, 0,0, Nothofagus::FontType::Hiragana);
    Nothofagus::TextureId textureId2 = canvas.addTexture(texture2);

    // makeTextTexture builds a tile-map text texture: one bellota, one draw call.
    std::string text = "- Nothofagus -";
    Nothofagus::IndirectTexture texture3 = Nothofagus::makeTextTexture(
        text, Nothofagus::FontType::Basic, {1.0, 1.0, 1.0, 1.0}, {0.0, 0.0, 0.0, 0.8});
    Nothofagus::TextureId textureId3 = canvas.addTexture(texture3);

    // Multi-line text plus a live setText counter (cheap map-only re-upload each second).
    Nothofagus::IndirectTexture texture4 = Nothofagus::makeTextTexture(
        "TILEMAP TEXT\nframe 0", Nothofagus::FontType::Basic,
        {1.0, 1.0, 0.4, 1.0}, {0.0, 0.0, 0.0, 0.8});
    Nothofagus::TextureId textureId4 = canvas.addTexture(texture4);

    // -1 will set the bellota at the back, so it does not draw over the other bellotas.
    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{75.0f, 50.0f}, 10.0}, textureId1, -1});

    // if you don't specify the offset, it is 0, the drawing order will be implementation defined.
    Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{75.0f, 90.0f}}, textureId2});
    Nothofagus::BellotaId bellotaId3 = canvas.addBellota({{{75.0f, 50.0f}}, textureId3});
    Nothofagus::BellotaId bellotaId4 = canvas.addBellota({{{75.0f, 20.0f}}, textureId4});

    float time = 0.0f;
    int seconds = 0;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota3 = canvas.bellota(bellotaId3);
        bellota3.transform().angle() = 5.0f * std::sin(0.005f * time);

        // Re-spell the second line each second. Same line/column count, so the bellota
        // does not need re-adding — only the cell grid re-uploads.
        const int nowSeconds = static_cast<int>(time / 1000.0f);
        if (nowSeconds != seconds)
        {
            seconds = nowSeconds;
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "TILEMAP TEXT\nframe %d", seconds % 100);
            Nothofagus::setText(std::get<Nothofagus::IndirectTexture>(canvas.texture(textureId4)), std::string(buffer));
            canvas.markTextureAsDirty(textureId4);
        }
    };
    
    canvas.run(update);
    
    return 0;
}