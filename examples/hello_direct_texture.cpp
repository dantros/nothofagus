#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>
#include <span>
#include <cstddef>

int main()
{
    Nothofagus::Canvas canvas({150, 100}, "Hello Direct Texture!", {0.7, 0.7, 0.7}, 6);

    Nothofagus::DirectTexture texture({ 5, 5 });
    texture.setColor(0,0, glm::vec4(0,0,0,1));
    texture.setColor(0,1, glm::vec4(1,1,1,1));
    texture.setColor(1,1, glm::vec4(1,0,0,1));
    texture.setColor(2,1, glm::vec4(0,1,0,1));
    texture.setColor(3,1, glm::vec4(0,0,1,1));
    texture.setColor(1,2, glm::vec4(1,1,0,1));
    texture.setColor(2,2, glm::vec4(0,1,1,1));
    texture.setColor(3,2, glm::vec4(1,0,1,1));
    texture.setColor(1,3, glm::vec4(0,0,0,1));
    texture.setColor(2,3, glm::vec4(0,0,0,1));
    texture.setColor(3,3, glm::vec4(1,1,1,1));
    Nothofagus::TextureId textureId = canvas.addTexture(texture);

    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{30.0f, 80.0f}, 4}, textureId});
    Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{0.0f, 50.0f}, 8}, textureId});
    Nothofagus::BellotaId bellotaId3 = canvas.addBellota({{{50.0f, 20.0f}, 4}, textureId});

    canvas.setTint(bellotaId3, {0.5, glm::vec3(1, 0, 0)});

    // This is an example to use texture memory that is not owned by Nothofagus itself 
    std::vector<std::uint8_t> externalTextureMemory{
        255,   0,   0, 255,   0,   0,   0, 255,  // Red, Black
          0,   0,   0, 255, 255,   0,   0, 255,  // Black, Red
    };

    // obtaining a view of the existing data via span
    std::span<std::uint8_t> textureDataSpan(externalTextureMemory.begin(), externalTextureMemory.end());

    // our Nothofagus texture now uses that pre-existing data
    Nothofagus::DirectTexture externalTexture(textureDataSpan, {2, 2});
    Nothofagus::TextureId textureId2 = canvas.addTexture(externalTexture);
    Nothofagus::BellotaId bellotaId4 = canvas.addBellota({{{80.0f, 80.0f}, 6}, textureId2});

    float time = 0.0f;
    bool rotate = true;
    bool visible = true;
    std::vector<glm::vec4> textureColors
    {
        {1,0,0,1},
        {1,1,0,1},
        {1,1,1,1},
        {0.5,0.5,0.5,1},
        {0.5,0.5,0,1},
        {0.25,0.25,0,1},
        {0.25,0,0,1}
    };
    std::size_t colorIndex = 0;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota2 = canvas.bellota(bellotaId2);
        bellota2.transform().location().x = 75.0f + 60.0f * std::sin(0.0005f * time);
    };
    
    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::SPACE, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        // rotating selection of new color
        const std::uint8_t newR = static_cast<std::uint8_t>(255.f * textureColors.at(colorIndex).r);
        const std::uint8_t newG = static_cast<std::uint8_t>(255.f * textureColors.at(colorIndex).g);
        const std::uint8_t newB = static_cast<std::uint8_t>(255.f * textureColors.at(colorIndex).b);
        const std::uint8_t newA = static_cast<std::uint8_t>(255.f * textureColors.at(colorIndex).a);

        colorIndex = (colorIndex + 1) % textureColors.size();

        // changing pixel colors top left and bottom right.
        externalTextureMemory.at( 4 * 0 + 0) = newR;
        externalTextureMemory.at( 4 * 0 + 1) = newG;
        externalTextureMemory.at( 4 * 0 + 2) = newB;
        externalTextureMemory.at( 4 * 0 + 3) = newA;

        externalTextureMemory.at( 4 * 3 + 0) = newR;
        externalTextureMemory.at( 4 * 3 + 1) = newG;
        externalTextureMemory.at( 4 * 3 + 2) = newB;
        externalTextureMemory.at( 4 * 3 + 3) = newA;

        // signaling that the texture was modified to re built it.
        canvas.markTextureAsDirty(textureId2);

        spdlog::info("Texture modified during runtime. Current color: ({}, {}, {}, {})", newR, newG, newB, newA);
    });
    canvas.run(update, controller);

    return 0;
}