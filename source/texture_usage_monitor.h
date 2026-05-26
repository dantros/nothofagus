#pragma once

#include <unordered_map>
#include <unordered_set>
#include "bellota.h"

namespace Nothofagus
{

class TextureUsageMonitor
{
public:
    TextureUsageMonitor() = default;

    bool addUnusedTexture(TextureId textureId);

    bool hasUnusedTexture(TextureId textureId) const;

    bool hasTexture(TextureId textureId) const;

    bool removeUnusedTexture(TextureId textureId);

    bool addEntry(BellotaId bellotaId, TextureId textureId);

    bool hasEntry(BellotaId bellotaId, TextureId textureId) const;

    bool removeEntry(BellotaId bellotaId, TextureId textureId);

    const std::unordered_set<TextureId>& getUnusedTextureIds() const;

    void clearUnusedTextureIds();

    std::size_t loadedTextures() const;

private:
    std::unordered_map<TextureId, std::unordered_set<BellotaId>> mTextureToBellotas;
    std::unordered_set<TextureId> mUnusedTextures;
};

}
