#pragma once

#include <glm/glm.hpp>
#include "bellota.h"
#include "texture.h"
#include "controller.h"
#include "tint.h"
#include <memory>
#include <functional>
#include <string>

namespace Nothofagus
{

struct ScreenSize
{
    unsigned int width, height;
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

    void run();
    void run(std::function<void(float deltaTime)> update);
    void run(std::function<void(float deltaTime)> update, Controller& controller);

    void close();

private:
    class CanvasImpl;
    std::unique_ptr<CanvasImpl> mCanvasImpl;
};

}