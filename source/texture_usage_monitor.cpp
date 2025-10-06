
#include "texture_usage_monitor.h"
#include "check.h"

namespace Nothofagus
{

bool TextureUsageMonitor::addUnusedTexture(TextureId textureId)
{
    auto pair = mUnusedTextures.insert(textureId);
    return pair.second;
}

bool TextureUsageMonitor::hasUnusedTexture(TextureId textureId) const
{
    return mUnusedTextures.contains(textureId);
}

bool TextureUsageMonitor::hasTexture(TextureId textureId) const
{
    return mUnusedTextures.contains(textureId) or mTextureToBellotas.contains(textureId);
}

bool TextureUsageMonitor::removeUnusedTexture(TextureId textureId)
{
    if (not mUnusedTextures.contains(textureId))
        return false;

    const std::size_t elementsErased = mUnusedTextures.erase(textureId);
    return elementsErased == 1;
}

bool TextureUsageMonitor::addEntry(BellotaId bellotaId, TextureId textureId)
{
    if (mTextureToBellotas.contains(textureId))
    {
        debugCheck(not mUnusedTextures.contains(textureId));
        std::unordered_set<BellotaId>& bellotaIds = mTextureToBellotas[textureId];
        auto pair = bellotaIds.insert(bellotaId);
        return pair.second; // true if insertion was successful.
    }
    
    debugCheck(mUnusedTextures.contains(textureId));

    mTextureToBellotas.emplace(textureId, std::unordered_set<BellotaId>{});
    std::unordered_set<BellotaId>& bellotaIds = mTextureToBellotas[textureId];
    auto pair = bellotaIds.insert(bellotaId);
    debugCheck(pair.second); // insertion must be successful

    const std::size_t erasedElements = mUnusedTextures.erase(textureId);
    debugCheck(erasedElements == 1);
    return true;
}

bool TextureUsageMonitor::hasEntry(BellotaId bellotaId, TextureId textureId) const
{
    if (not mTextureToBellotas.contains(textureId))
        return false;

    const std::unordered_set<BellotaId>& bellotaIds = mTextureToBellotas.at(textureId);

    return bellotaIds.contains(bellotaId);
}

bool TextureUsageMonitor::removeEntry(BellotaId bellotaId, TextureId textureId)
{
    if (not hasEntry(bellotaId, textureId))
        return false;

    std::unordered_set<BellotaId>& bellotaIds = mTextureToBellotas[textureId];
    debugCheck(bellotaIds.contains(bellotaId));

    bellotaIds.erase(bellotaId);

    if (bellotaIds.empty())
    {
        const std::size_t elementsErased = mTextureToBellotas.erase(textureId);
        debugCheck(elementsErased == 1);
        auto pair = mUnusedTextures.insert(textureId);
        debugCheck(pair.second);
    }

    return true;
}

const std::unordered_set<TextureId>& TextureUsageMonitor::getUnusedTextureIds() const
{
    return mUnusedTextures;
}

void TextureUsageMonitor::clearUnusedTextureIds()
{
    mUnusedTextures.clear();
}

std::size_t TextureUsageMonitor::loadedTextures() const
{
    return mUnusedTextures.size() + mTextureToBellotas.size();
}

}