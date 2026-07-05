#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <format>
#include <vector>
#include <nothofagus.h>

int generateRandomInt(int min, int max)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(min, max);
    return dist(rng);
}

float generateRandomFloat(float min, float max)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<> dist(min, max);
    return dist(rng);
}

glm::vec2 generateRandomPosition(unsigned int width, unsigned int height)
{
    return {generateRandomFloat(0, width), generateRandomFloat(0, height)};
}

Nothofagus::TextureId addTextureWithText(Nothofagus::Canvas& canvas, Nothofagus::ColorPallete pallete, const std::string& text)
{
    const glm::vec4 bgColor = pallete.colors.empty() ? glm::vec4(0.0f) : pallete.colors[0];
    const glm::vec4 fgColor = pallete.colors.size() > 1 ? pallete.colors[1] : glm::vec4(1.0f);
    Nothofagus::IndirectTexture texture = Nothofagus::makeTextTexture(text, Nothofagus::FontType::Basic, fgColor, bgColor);
    return canvas.addTexture(texture);
}

Nothofagus::BellotaId addBellotaWithWrittenId(Nothofagus::Canvas& canvas, Nothofagus::TextureId dummyTextureId, Nothofagus::ColorPallete pallete, glm::vec2 position)
{
    /* We do not know the bellota id before inserting the bellota into the canvas, so we insert the
     * bellota with a fallback texture, and then change it for the new one with the text
     */
    Nothofagus::BellotaId bellotaId = canvas.addBellota({{position}, dummyTextureId});
    Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);

    // This is to ensure bellotas are drawn in order according to their id. This way, transparency allow proper visibility.
    bellota.depthOffset() = -127 + bellotaId.id;

    const std::string text = std::format("{}", bellotaId.id);
    Nothofagus::TextureId textureId = addTextureWithText(canvas, pallete, text);
    canvas.setTexture(bellotaId, textureId);

    return bellotaId;
}

int main()
{
    Nothofagus::ScreenSize screenSize{150, 100};
    Nothofagus::Canvas canvas(screenSize, "Creating and destroying bellotas", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.8},
        {1.0, 1.0, 1.0, 0.8 }
    };

    Nothofagus::IndirectTexture dummyTexture({2, 2}, {1.0, 1.0, 1.0, 1.0});
    Nothofagus::TextureId dummyTextureId = canvas.addTexture(dummyTexture);

    // this invisible Bellota is just to keep the texture alive, as Nothofagus removes unused textures every frame.
    Nothofagus::BellotaId dummyBellotaId = canvas.addBellota({{{10.0, 10.0}}, dummyTextureId});
    Nothofagus::Bellota& dummyBellota = canvas.bellota(dummyBellotaId);
    dummyBellota.visible() = false;

    std::vector<Nothofagus::BellotaId> bellotaIds;

    // ImGui — runs on the sim-UI context (sim thread). Reads bellotaIds, which the
    // sim-thread controller actions mutate; both are sim-side, so no atomics needed.
    auto ui = [&](float)
    {
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("Use W to create and S to destroy");
        ImGui::Text("Handling %zu bellotas", bellotaIds.size());
        ImGui::End();
    };

    // Game input — dispatched on the sim thread; creates/destroys bellotas in the live scene.
    Nothofagus::Controller simController;
    simController.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        glm::vec2 randomPosition = generateRandomPosition(screenSize.width, screenSize.height);
        Nothofagus::BellotaId newBellotaId = addBellotaWithWrittenId(canvas, dummyTextureId, pallete, randomPosition);
        bellotaIds.push_back(newBellotaId);

        spdlog::info("Bellota {} created!", newBellotaId.id);
    });
    simController.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        if (bellotaIds.empty())
            return;
        
        std::size_t bellotaIndexToDelete = generateRandomInt(0, bellotaIds.size() - 1);
        Nothofagus::BellotaId bellotaIdToDelete = bellotaIds.at(bellotaIndexToDelete);
        canvas.removeBellota(bellotaIdToDelete);

        // replacing the element that we wish to remove for the last one and then removing the last one.
        bellotaIds.at(bellotaIndexToDelete) = bellotaIds.back();
        bellotaIds.pop_back();

        spdlog::info("Bellota {} destroyed :(", bellotaIdToDelete.id);
    });

    // Multithreaded convenience: sim thread runs ui + simController; the main thread renders.
    // No window input, so renderController is empty.
    Nothofagus::Controller renderController;
    canvas.run([](float){}, ui, simController, renderController);

    return 0;
}