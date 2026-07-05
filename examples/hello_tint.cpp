#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <numbers>
#include <nothofagus.h>
#include <glm/gtc/type_ptr.hpp>

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
    
    Nothofagus::IndirectTexture textureGreen({8, 8}, {0.5, 0.5, 0.5, 1.0});
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

    Nothofagus::IndirectTexture textureRed = textureGreen;
    textureRed.setPallete(palleteRed);

    Nothofagus::IndirectTexture textureBlue = textureGreen;
    textureBlue.setPallete(palleteBlue);

    Nothofagus::TextureId textureIdGreen = canvas.addTexture(textureGreen);
    Nothofagus::TextureId textureIdRed = canvas.addTexture(textureRed);
    Nothofagus::TextureId textureIdBlue = canvas.addTexture(textureBlue);

    Nothofagus::BellotaId bellotaIdGreen = canvas.addBellota({ {{100.0f, 50.0f}}, textureIdGreen });
    Nothofagus::BellotaId bellotaIdRed = canvas.addBellota({ {{100.0f, 50.0f}}, textureIdRed });
    Nothofagus::BellotaId bellotaIdBlue = canvas.addBellota({ {{100.0f, 50.0f}}, textureIdBlue });

    float time = 0.0f;
    float intensity = 0.0f;
    float tintColor[3] = { 1.0f, 1.0f, 1.0f };

    // Game logic — runs on the sim thread (no ImGui here). Reads intensity/tintColor,
    // which the ui writes; both callbacks run on the sim thread, so plain floats are safe.
    auto update = [&](float dt)
    {
        time += dt;

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

        canvas.setTint(bellotaIdGreen, { intensity, glm::make_vec3(tintColor) });

        canvas.setTint(bellotaIdRed, { std::abs(std::sin(0.005f * time)), {1.0, 1.0, 1.0} });
    };

    // Interactive ImGui — runs on the sim-UI context (sim thread), cloned to the render thread.
    auto ui = [&](float)
    {
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Green Tint");
        ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f);
        ImGui::ColorPicker3("Color", tintColor);
        ImGui::End();
    };

    canvas.run(update, ui);

    return 0;
}