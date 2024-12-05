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
    
    AnimatedBellotaId addAnimatedBellota(const AnimatedBellota& animatedBellota);
    void removeAnimatedBellota(const AnimatedBellotaId animatedBellotaId);

    TextureId addTexture(const Texture& texture);
    void removeTexture(const TextureId textureId);
    
    TextureArrayId addTextureArray(const TextureArray& textureArray);
    void removeTextureArray(const TextureArrayId textureArrayId);

    void setTint(const BellotaId bellotaId, const Tint& tint);
    void removeTint(const BellotaId bellotaId);

    Bellota& bellota(BellotaId bellotaId);
    const Bellota& bellota(BellotaId bellotaId) const;
    
    AnimatedBellota& animatedBellota(AnimatedBellotaId animatedBellotaId);
    const AnimatedBellota& animatedBellota(AnimatedBellotaId animatedBellotaId) const;

    Texture& texture(TextureId textureId);
    const Texture& texture(TextureId textureId) const;
    
    TextureArray& textureArray(TextureArrayId textureArrayId);
    const TextureArray& textureArray(TextureArrayId textureArrayId) const;

    bool& stats();
    const bool& stats() const;

    void run(std::function<void(float deltaTime)> update, Controller& controller);

    void close();

private:
    ScreenSize mScreenSize;
    std::string mTitle;
    glm::vec3 mClearColor;
    unsigned int mPixelSize;

    TextureContainer mTextures;
    TextureArrayContainer mTexturesArrays;

    BellotaContainer mBellotas;
    AnimatedBellotaContainer mAnimatedBellotas;

    unsigned int mShaderProgram;
    unsigned int mAnimatedShaderProgram;

    bool mStats;

    // Forward declaration to interface with third party libs
    struct Window;
    std::unique_ptr<Window> mWindow;
};

}