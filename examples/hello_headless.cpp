#include <canvas.h>
#include <texture.h>
#include <bellota.h>
#include <iostream>
#include <iomanip>
#include <cstdint>

int main()
{
    constexpr unsigned int canvasWidth  = 15;
    constexpr unsigned int canvasHeight = 10;

    Nothofagus::Canvas canvas(
        {canvasWidth, canvasHeight},
        "headless demo",
        {0.0f, 0.0f, 0.0f},
        1,
        14,
        true // headless
    );

    // 3x3 red square
    Nothofagus::ColorPallete redPalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture redTex({3, 3}, {0.0f, 0.0f, 0.0f, 0.0f});
    redTex.setPallete(redPalette);
    redTex.setPixels({
        1, 1, 1,
        1, 1, 1,
        1, 1, 1,
    });

    // 3x2 green rectangle
    Nothofagus::ColorPallete greenPalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    });
    Nothofagus::IndirectTexture greenTex({3, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    greenTex.setPallete(greenPalette);
    greenTex.setPixels({
        1, 1, 1,
        1, 1, 1,
    });

    // 2x2 blue square
    Nothofagus::ColorPallete bluePalette({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 1.0f}
    });
    Nothofagus::IndirectTexture blueTex({2, 2}, {0.0f, 0.0f, 0.0f, 0.0f});
    blueTex.setPallete(bluePalette);
    blueTex.setPixels({
        1, 1,
        1, 1,
    });

    auto redTexId   = canvas.addTexture(redTex);
    auto greenTexId = canvas.addTexture(greenTex);
    auto blueTexId  = canvas.addTexture(blueTex);

    // Place bellotas. Transform location is the bottom-left corner of the texture
    // shifted by half the texture size (the mesh is centered on the origin).
    // For a 3x3 texture at bottom-left (1,1): location = (1 + 1.5, 1 + 1.5) = (2.5, 2.5)
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(2.5f, 2.5f)}, redTexId));

    // Green 3x2 at bottom-left (6, 5): location = (6 + 1.5, 5 + 1.0) = (7.5, 6.0)
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(7.5f, 6.0f)}, greenTexId));

    // Blue 2x2 at bottom-left (11, 7): location = (11 + 1.0, 7 + 1.0) = (12.0, 8.0)
    canvas.addBellota(Nothofagus::Bellota({glm::vec2(12.0f, 8.0f)}, blueTexId));

    // Run a few frames so GPU resources get uploaded and rendered
    for (int i = 0; i < 3; ++i)
        canvas.tick(16.0f);

    // Capture screenshot
    Nothofagus::DirectTexture screenshot = canvas.takeScreenshot();
    Nothofagus::TextureData textureData = screenshot.generateTextureData();
    std::span<std::uint8_t> pixels = textureData.getDataSpan();
    const int width  = static_cast<int>(textureData.width());
    const int height = static_cast<int>(textureData.height());

    std::cout << "Screenshot: " << width << "x" << height << std::endl;
    std::cout << std::endl;

    // Print top-to-bottom (TextureData from screenshot is already top-to-bottom)
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const int index = (y * width + x) * 4;
            const std::uint8_t r = pixels[index + 0];
            const std::uint8_t g = pixels[index + 1];
            const std::uint8_t b = pixels[index + 2];
            const std::uint32_t hexColor = (r << 16) | (g << 8) | b;
            std::cout << "0x" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << hexColor;
            if (x < width - 1)
                std::cout << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
