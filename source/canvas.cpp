#include "canvas.h"
#include "canvas_impl.h"

namespace Nothofagus
{

Canvas::Canvas(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize
)
{
    mCanvasImpl = std::make_unique<CanvasImpl>(screenSize, title, clearColor, pixelSize);
}

Canvas::~Canvas()
{

}

const ScreenSize& Canvas::screenSize() const
{
    return mCanvasImpl->screenSize();
}

BellotaId Canvas::addBellota(const Bellota& bellota)
{
    return mCanvasImpl->addBellota(bellota);
}

void Canvas::removeBellota(const BellotaId bellotaId)
{
    mCanvasImpl->removeBellota(bellotaId);
}

TextureId Canvas::addTexture(const Texture& texture)
{
    return mCanvasImpl->addTexture(texture);
}

void Canvas::removeTexture(const TextureId textureId)
{
    mCanvasImpl->removeTexture(textureId);
}

void Canvas::clearUnusedTextures()
{
    mCanvasImpl->clearUnusedTextures();
}

void Canvas::setTexture(const BellotaId bellotaId, const TextureId textureId)
{
    mCanvasImpl->setTexture(bellotaId, textureId);
}

TextureId Canvas::addTextureArray(const Texture& textureArray)
{
    return mCanvasImpl->addTexture(textureArray);
}

void Canvas::removeTextureArray(const TextureId textureId)
{
    mCanvasImpl->removeTexture(textureId);
}

void Canvas::setTint(const BellotaId bellotaId, const Tint& tint)
{
    mCanvasImpl->setTint(bellotaId, tint);
}

void Canvas::removeTint(const BellotaId bellotaId)
{
    mCanvasImpl->removeTint(bellotaId);
}

Bellota& Canvas::bellota(BellotaId bellotaId)
{
    return mCanvasImpl->bellota(bellotaId);
}

const Bellota& Canvas::bellota(BellotaId bellotaId) const
{
    return mCanvasImpl->bellota(bellotaId);
}

Texture& Canvas::texture(TextureId textureId)
{
    return mCanvasImpl->texture(textureId);
}

const Texture& Canvas::texture(TextureId textureId) const
{
    return mCanvasImpl->texture(textureId);
}

bool& Canvas::stats()
{
    return mCanvasImpl->stats();
}

const bool& Canvas::stats() const
{
    return mCanvasImpl->stats();
}

void Canvas::run()
{
    auto update = [](float deltaTime){};
    Controller controller;
    mCanvasImpl->run(update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update)
{
    Controller controller;
    mCanvasImpl->run(update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    mCanvasImpl->run(update, controller);
}

void Canvas::close()
{
    mCanvasImpl->close();
}

}