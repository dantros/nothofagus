#include "canvas.h"
#include "canvas_impl.h"

namespace Nothofagus
{

Canvas::Canvas(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize,
    const float imguiFontSize,
    bool headless
)
{
    mCanvasImpl = std::make_unique<CanvasImpl>(screenSize, title, clearColor, pixelSize, imguiFontSize, headless);
}

Canvas::~Canvas()
{

}

std::size_t Canvas::getCurrentMonitor() const
{
    return mCanvasImpl->getCurrentMonitor();;
}

bool Canvas::isFullscreen() const
{
    return mCanvasImpl->isFullscreen();
}

void Canvas::setFullScreenOnMonitor(std::size_t monitor)
{
    mCanvasImpl->setFullScreenOnMonitor(monitor);
}

void Canvas::setWindowed()
{
    mCanvasImpl->setWindowed();
}

const ScreenSize& Canvas::screenSize() const
{
    return mCanvasImpl->screenSize();
}

void Canvas::setScreenSize(const ScreenSize& screenSize)
{
    mCanvasImpl->setScreenSize(screenSize);
}

void Canvas::setClearColor(glm::vec3 clearColor)
{
    mCanvasImpl->setClearColor(clearColor);
}

void Canvas::setAutoRemoveUnusedTextures(bool enabled)
{
    mCanvasImpl->setAutoRemoveUnusedTextures(enabled);
}

void Canvas::setWindowTitle(const std::string& title)
{
    mCanvasImpl->setWindowTitle(title);
}

ScreenSize Canvas::windowSize() const
{
    return mCanvasImpl->windowSize();
}

ViewportRect Canvas::gameViewport() const
{
    return mCanvasImpl->gameViewport();
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

void Canvas::setTexture(const BellotaId bellotaId, const TextureId textureId)
{
    mCanvasImpl->setTexture(bellotaId, textureId);
}

void Canvas::markTextureAsDirty(const TextureId textureId)
{
    mCanvasImpl->markTextureAsDirty(textureId);
}

void Canvas::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)
{
    mCanvasImpl->setTextureMinFilter(textureId, mode);
}

void Canvas::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)
{
    mCanvasImpl->setTextureMagFilter(textureId, mode);
}

RenderTargetId Canvas::addRenderTarget(ScreenSize size)
{
    return mCanvasImpl->addRenderTarget(size);
}

void Canvas::removeRenderTarget(RenderTargetId renderTargetId)
{
    mCanvasImpl->removeRenderTarget(renderTargetId);
}

TextureId Canvas::renderTargetTexture(RenderTargetId renderTargetId) const
{
    return mCanvasImpl->renderTargetTexture(renderTargetId);
}

void Canvas::renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)
{
    mCanvasImpl->renderTo(renderTargetId, std::move(bellotaIds));
}

void Canvas::renderImguiTo(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback)
{
    mCanvasImpl->renderImguiTo(renderTargetId, std::move(imguiDrawCallback));
}

void Canvas::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor)
{
    mCanvasImpl->setRenderTargetClearColor(renderTargetId, clearColor);
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

void Canvas::tick(float deltaTime, std::function<void(float)> update, Controller& controller)
{
    mCanvasImpl->tick(deltaTime, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update)
{
    mCanvasImpl->tick(deltaTime, update);
}

void Canvas::tick(float deltaTime)
{
    mCanvasImpl->tick(deltaTime);
}

void Canvas::close()
{
    mCanvasImpl->close();
}

DirectTexture Canvas::takeScreenshot() const
{
    return mCanvasImpl->takeScreenshot();
}

}