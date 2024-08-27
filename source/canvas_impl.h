#pragma once

#include "canvas.h"
#include "texture_container.h"
#include "bellota_container.h"

namespace Nothofagus
{

class Canvas::CanvasImpl
{
public:
    CanvasImpl(
        const ScreenSize& screenSize,
        const std::string& title,
        const glm::vec3 clearColor,
        const unsigned int pixelSize
    );

    ~CanvasImpl();

    const ScreenSize& screenSize() const;

    BellotaId addBellota(const Bellota& bellota);

    void removeBellota(const BellotaId bellotaId);

    TextureId addTexture(const Texture& texture);

    void removeTexture(const TextureId textureId);

    Bellota& bellota(BellotaId bellotaId);

    const Bellota& bellota(BellotaId bellotaId) const;

    Texture& texture(TextureId textureId);

    const Texture& texture(TextureId textureId) const;

    void run(std::function<void(float deltaTime)> update, Controller& controller);

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