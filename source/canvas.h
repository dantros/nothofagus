#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "bellota.h"
#include "texture.h"
#include "index_factory.h"
#include "indexed_container.h"

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

constexpr static ScreenSize DEFAULT_SCREEN_SIZE{800, 600};
const std::string DEFAULT_TITLE{"Nothofagus App"};
const glm::vec3 DEFAULT_CLEAR_COLOR{0.0f, 0.0f, 0.0f};

class Canvas
{
private:
    void init();

public:
    Canvas(
        const ScreenSize& screenSize = DEFAULT_SCREEN_SIZE,
        const std::string& title = DEFAULT_TITLE,
        const glm::vec3 clearColor = DEFAULT_CLEAR_COLOR
    ):
        mScreenSize(screenSize),
        mTitle(title),
        mClearColor(clearColor)
    {}

    ~Canvas() = default;

    BellotaId addBellota(const Bellota& bellota)
    {
        return {mBellotas.add(bellota)};
    }

    void removeBellota(const BellotaId bellotaId)
    {
        mBellotas.remove(bellotaId.id);
    }

    TextureId addTexture(const Texture& texture)
    {
        return {mTextures.add(texture)};
    }

    void removeTexture(const TextureId textureId)
    {
        mTextures.remove(textureId.id);
    }

    Bellota& bellota(BellotaId bellotaId)
    {
        return mBellotas.at(bellotaId.id);
    }

    const Bellota& bellota(BellotaId bellotaId) const
    {
        return mBellotas.at(bellotaId.id);
    }

    Texture& texture(TextureId textureId)
    {
        return mTextures.at(textureId.id);
    }

    const Texture& texture(TextureId textureId) const
    {
        return mTextures.at(textureId.id);
    }

    //void tick(Controller& controller, std::function<void(float deltaTime)> updateFunction);

    void close();

private:
    ScreenSize mScreenSize;
    std::string mTitle;
    glm::vec3 mClearColor;

    using BellotaContainer = IndexedContainer<Bellota>;
    using TextureContainer = IndexedContainer<Texture>;

    TextureContainer mTextures;
    BellotaContainer mBellotas;

    // Forward declaration to interface with third party libs
    /*struct Window;
    std::unique_ptr<Window> mWindow;*/
};

}