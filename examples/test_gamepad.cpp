#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <algorithm>
#include <nothofagus.h>

int main()
{
    Nothofagus::ScreenSize screenSize{200, 150};
    Nothofagus::Canvas canvas(screenSize, "Gamepad test", {0.15f, 0.15f, 0.2f}, 6);

    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},
        {0.2, 0.5, 0.9, 1.0},
        {0.4, 0.7, 1.0, 1.0},
        {0.6, 0.9, 1.0, 1.0},
    };

    Nothofagus::IndirectTexture texture({8, 8}, {0.5, 0.5, 0.5, 1.0});
    texture.setPallete(pallete)
        .setPixels(
        {
            0,0,3,3,3,3,0,0,
            0,3,2,2,2,2,3,0,
            3,2,1,2,2,1,2,3,
            3,2,2,2,2,2,2,3,
            3,2,2,2,2,2,2,3,
            3,2,1,2,2,1,2,3,
            0,3,2,2,2,2,3,0,
            0,0,3,3,3,3,0,0,
        }
    );
    Nothofagus::TextureId textureId = canvas.addTexture(texture);
    Nothofagus::BellotaId bellotaId = canvas.addBellota({{{100.0f, 75.0f}}, textureId});

    float time = 0.0f;
    constexpr float horizontalSpeed = 0.12f;
    constexpr float angularSpeed = 0.1f;
    constexpr float discreteStep = 10.0f;
    bool rotate = false;
    float pulseTimer = 0.0f;

    Nothofagus::Controller controller;

    // Keyboard: Escape to quit
    controller.registerAction({Nothofagus::Key::ESCAPE, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.close(); });

    // Gamepad: A button pulse
    controller.registerGamepadAction({0, Nothofagus::GamepadButton::A, Nothofagus::DiscreteTrigger::Press},
        [&]() { pulseTimer = 500.0f; });

    // Gamepad: Start toggles rotation
    controller.registerGamepadAction({0, Nothofagus::GamepadButton::Start, Nothofagus::DiscreteTrigger::Press},
        [&]() { rotate = not rotate; });

    // Gamepad: D-pad discrete movement
    controller.registerGamepadAction({0, Nothofagus::GamepadButton::DpadUp, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().y += discreteStep; });
    controller.registerGamepadAction({0, Nothofagus::GamepadButton::DpadDown, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().y -= discreteStep; });
    controller.registerGamepadAction({0, Nothofagus::GamepadButton::DpadLeft, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().x -= discreteStep; });
    controller.registerGamepadAction({0, Nothofagus::GamepadButton::DpadRight, Nothofagus::DiscreteTrigger::Press},
        [&]() { canvas.bellota(bellotaId).transform().location().x += discreteStep; });

    auto update = [&](float deltaTime)
    {
        time += deltaTime;

        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);

        // Smooth movement via left stick (polling)
        std::vector<int> connectedIds = controller.getConnectedGamepadIds();
        if (not connectedIds.empty())
        {
            int gamepadId = connectedIds[0];
            float leftX = controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftX);
            float leftY = controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftY);
            bellota.transform().location().x += leftX * horizontalSpeed * deltaTime;
            bellota.transform().location().y += leftY * horizontalSpeed * deltaTime;
        }

        // Clamp to screen
        bellota.transform().location().x = std::clamp(bellota.transform().location().x, 10.0f, static_cast<float>(screenSize.width) - 10.0f);
        bellota.transform().location().y = std::clamp(bellota.transform().location().y, 10.0f, static_cast<float>(screenSize.height) - 10.0f);

        // Rotation
        if (rotate)
            bellota.transform().angle() += angularSpeed * deltaTime;

        // Pulse effect from A button
        if (pulseTimer > 0.0f)
        {
            pulseTimer -= deltaTime;
            float pulse = 1.0f + 0.5f * std::sin(pulseTimer * 0.02f);
            bellota.transform().scale() = glm::vec2(pulse * 3.0f, pulse * 3.0f);
        }
        else
        {
            bellota.transform().scale() = glm::vec2(3.0f, 3.0f);
        }

        // ImGui status
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Gamepad Test");
        ImGui::Text("Left stick: move sprite");
        ImGui::Text("D-pad: discrete movement");
        ImGui::Text("A: pulse | Start: toggle rotation");
        ImGui::Text("Escape: quit");
        ImGui::Separator();
        if (connectedIds.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No gamepads connected");
        }
        else
        {
            std::string idList;
            for (int id : connectedIds)
            {
                if (not idList.empty()) idList += ", ";
                idList += std::to_string(id);
            }
            ImGui::Text("Connected: [%s]", idList.c_str());

            int gamepadId = connectedIds[0];
            ImGui::Text("LX: %.2f  LY: %.2f",
                controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftX),
                controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftY));
            ImGui::Text("RX: %.2f  RY: %.2f",
                controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::RightX),
                controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::RightY));
            ImGui::Text("LT: %.2f  RT: %.2f",
                controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::LeftTrigger),
                controller.getGamepadAxis(gamepadId, Nothofagus::GamepadAxis::RightTrigger));
        }
        ImGui::End();
    };

    canvas.run(update, controller);

    return 0;
}
