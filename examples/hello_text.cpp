#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <random>
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

    Nothofagus::Texture texture1({ 15, 10 }, { 0.5, 0.5, 0.5, 1.0 });
    texture1.setPallete(pallete1);
    {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, pallete1.size()-1);

        for (std::size_t i = 0; i < texture1.size().x; ++i)
        {
            for (std::size_t j = 0; j < texture1.size().y; ++j)
            {
                const Nothofagus::Pixel randomColor{ dist(rng) };
                texture1.setPixel(i, j, randomColor);
            }
        }
    }

    Nothofagus::TextureId textureId1 = canvas.addTexture(texture1);

    Nothofagus::Texture texture2({ 8, 8 }, { 0.5, 0.5, 0.5, 1.0 });
    texture2.setPallete(pallete2);
    Nothofagus::writeChar(texture2, 0xD, 0,0, Nothofagus::FontType::Hiragana);
    Nothofagus::TextureId textureId2 = canvas.addTexture(texture2);

    std::string text = "- Nothofagus -";
    Nothofagus::Texture texture3({8 * text.size(), 8}, {0.5, 0.5, 0.5, 1.0});
    texture3.setPallete(pallete3);
    Nothofagus::writeText(texture3, text);
    Nothofagus::TextureId textureId3 = canvas.addTexture(texture3);

    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{75.0f, 50.0f}, 10.0}, textureId1});
    Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{75.0f, 90.0f}}, textureId2});
    Nothofagus::BellotaId bellotaId3 = canvas.addBellota({{{75.0f, 50.0f}}, textureId3});

    float time = 0.0f;
    bool rotate = true;
    bool visible = true;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota3 = canvas.bellota(bellotaId3);
        bellota3.transform().angle() = 5.0f * std::sin(0.005f * time);
    };
    
    canvas.run(update);
    
    return 0;
}