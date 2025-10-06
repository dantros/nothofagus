#pragma once

#include <unordered_map>
#include <unordered_set>
//#include <utility>
#include "bellota.h"

namespace std
{

template<>
struct hash<Nothofagus::TextureId>
{
    std::size_t operator()(const Nothofagus::TextureId& textureId) const
    {
        return std::hash<std::size_t>{}(textureId.id);
    }
};

template<>
struct hash<Nothofagus::BellotaId>
{
    std::size_t operator()(const Nothofagus::BellotaId& bellotaId) const
    {
        return std::hash<std::size_t>{}(bellotaId.id);
    }
};

}

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