#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <numbers>
#include <nothofagus.h>
#include <glm/gtc/type_ptr.hpp>b

int main()
{
    Nothofagus::Canvas canvas({150, 100}, "Hello Tint", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete palleteGreen{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.4, 0.0, 1.0},
        {0.2, 0.8, 0.2, 1.0},
        {0.5, 1.0, 0.5, 1.0},
    };
    Nothofagus::ColorPallete palleteRed{
        {0.0, 0.0, 0.0, 0.0},
        {0.4, 0.0, 0.0, 1.0},
        {0.8, 0.2, 0.2, 1.0},
        {1.0, 0.5, 0.5, 1.0},
    };
    Nothofagus::ColorPallete palleteBlue{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.4, 1.0},
        {0.2, 0.2, 0.8, 1.0},
        {0.5, 0.5, 1.0, 1.0},
    };
    
    Nothofagus::Texture textureGreen({8, 8}, {0.5, 0.5, 0.5, 1.0});
    textureGreen.setPallete(palleteGreen)
        .setPixels(
        {
            2,1,3,0,0,3,2,1,
            2,1,1,0,0,0,2,1,
            2,1,1,1,0,0,2,1,
            2,1,2,1,1,0,2,1,
            2,1,0,2,1,1,2,1,
            2,1,0,0,2,1,2,1,
            2,1,0,0,0,2,2,1,
            2,1,3,0,0,3,2,1,
        }
    );

    Nothofagus::Texture textureRed = textureGreen;
    textureRed.setPallete(palleteRed);

    Nothofagus::Texture textureBlue = textureGreen;
    textureBlue.setPallete(palleteBlue);

    Nothofagus::TextureId textureIdGreen = canvas.addTexture(textureGreen);
    Nothofagus::TextureId textureIdRed = canvas.addTexture(textureRed);
    Nothofagus::TextureId textureIdBlue = canvas.addTexture(textureBlue);

    Nothofagus::BellotaId bellotaIdGreen = canvas.addBellota({ {{100.0f, 50.0f}}, textureIdGreen });
    Nothofagus::BellotaId bellotaIdRed = canvas.addBellota({ {{100.0f, 50.0f}}, textureIdRed });
    Nothofagus::BellotaId bellotaIdBlue = canvas.addBellota({ {{100.0f, 50.0f}}, textureIdBlue });

    float time = 0.0f;
    bool rotate = true;
    float intensity = 0.0f;
    float tintColor[3] = { 1.0f, 1.0f, 1.0f };

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellotaRed = canvas.bellota(bellotaIdRed);

        Nothofagus::Bellota& bellotaGreen = canvas.bellota(bellotaIdGreen);
        bellotaGreen.transform().location() = glm::vec2(100.0f, 50.0f)
            + 20.0f * glm::vec2(
                std::cos(0.001f * time),
                std::sin(0.001f * time)
            );

        Nothofagus::Bellota& bellotaBlue = canvas.bellota(bellotaIdBlue);
        bellotaBlue.transform().location() = glm::vec2(100.0f, 50.0f)
            + 20.0f * glm::vec2(
                std::cos(0.001f * time + std::numbers::pi/4),
                std::sin(0.001f * time + std::numbers::pi/4)
            );

        ImGui::Begin("Green Tint");
        ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f);
        ImGui::ColorPicker3("Color", tintColor);
        ImGui::End();

        canvas.setTint(bellotaIdGreen, { intensity, glm::make_vec3(tintColor) });

        canvas.setTint(bellotaIdRed, { std::abs(std::sin(0.005f * time)), {1.0, 1.0, 1.0} });
    };
    
    canvas.run(update);
    
    return 0;
}