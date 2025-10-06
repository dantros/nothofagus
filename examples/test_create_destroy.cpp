#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
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
    Nothofagus::IndirectTexture texture({10 * text.size(), 10}, {0.5, 0.5, 0.5, 1.0});
    texture.setPallete(pallete);
    Nothofagus::writeText(texture, text, 1,1, Nothofagus::FontType::Basic);
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

    auto update = [&](float dt)
    {
        ImGui::Begin("Use W to create and S to destroy");
        ImGui::Text("Handling %d bellotas", bellotaIds.size());
        ImGui::End();
    };

    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        glm::vec2 randomPosition = generateRandomPosition(screenSize.width, screenSize.height);
        Nothofagus::BellotaId newBellotaId = addBellotaWithWrittenId(canvas, dummyTextureId, pallete, randomPosition);
        bellotaIds.push_back(newBellotaId);

        spdlog::info("Bellota {} created!", newBellotaId.id);
    });
    controller.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press}, [&]()
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
    
    canvas.run(update, controller);
    
    return 0;
}