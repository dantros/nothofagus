#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <optional>
#include "bellota.h"
#include "texture.h"
#include "index_factory.h"
#include "indexed_container.h"
#include "bellota_container.h"
#include "texture_container.h"
#include <memory>
#include <functional>

namespace Nothofagus
{

struct ScreenSize
{
    unsigned int width, height;
};

struct BellotaId
{
    std::size_t id;
};

constexpr static ScreenSize DEFAULT_SCREEN_SIZE{256, 240};
const std::string DEFAULT_TITLE{"Nothofagus App"};
const glm::vec3 DEFAULT_CLEAR_COLOR{0.0f, 0.0f, 0.0f};
constexpr static unsigned int DEFAULT_PIXEL_SIZE{ 4 }; // Each pixel is represented by this quantity of screen pixels

class Canvas
{
public:
    Canvas(
        const ScreenSize& screenSize = DEFAULT_SCREEN_SIZE,
        const std::string& title = DEFAULT_TITLE,
        const glm::vec3 clearColor = DEFAULT_CLEAR_COLOR,
        const unsigned int pixelSize = DEFAULT_PIXEL_SIZE
    );

    ~Canvas();

    BellotaId addBellota(const Bellota& bellota)
    {
        return {mBellotas.add({bellota, std::nullopt, std::nullopt})};
    }

    void removeBellota(const BellotaId bellotaId)
    {
        mBellotas.remove(bellotaId.id);
    }

    TextureId addTexture(const Texture& texture)
    {
        return { mTextures.add({texture, std::nullopt}) };
    }

    void removeTexture(const TextureId textureId)
    {
        mTextures.remove(textureId.id);
    }

    Bellota& bellota(BellotaId bellotaId)
    {
        return mBellotas.at(bellotaId.id).bellota;
    }

    const Bellota& bellota(BellotaId bellotaId) const
    {
        return mBellotas.at(bellotaId.id).bellota;
    }

    Texture& texture(TextureId textureId)
    {
        return mTextures.at(textureId.id).texture;
    }

    const Texture& texture(TextureId textureId) const
    {
        return mTextures.at(textureId.id).texture;
    }

    //void tick(Controller& controller, std::function<void(float deltaTime)> updateFunction);

    void run(std::function<void(float deltaTime)> update);

    void close();

private:
    ScreenSize mScreenSize;
    std::string mTitle;
    glm::vec3 mClearColor;
    unsigned int mPixelSize;

    TextureContainer mTextures;
    BellotaContainer mBellotas;

    unsigned int mShaderProgram;

    // Forward declaration to interface with third party libs
    struct Window;
    std::unique_ptr<Window> mWindow;
};

}