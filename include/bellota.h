#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include "transform.h"
#include "mesh.h"

namespace Nothofagus
{

struct TextureId
{
    std::size_t id;

    bool operator==(const TextureId& rhs) const
    {
        return id == rhs.id;
    }
};

struct BellotaId
{
    std::size_t id;

    bool operator==(const BellotaId& rhs) const
    {
        return id == rhs.id;
    }
};

}

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

/* Drawable element */
class Bellota
{
public:
    Bellota(Transform transform, TextureId textureId):
        mTransform( transform ),
        mTextureId{ textureId },
        mMeshId{ std::nullopt },
        mCurrentLayer{ 0 },
        mDepthOffset{ 0 },
        mVisible{ true },
        mOpacity{ 1.0f }
    {
    }

    Bellota(Transform transform, TextureId textureId, std::int8_t depthOffset) :
        mTransform( transform ),
        mTextureId{ textureId },
        mMeshId{ std::nullopt },
        mCurrentLayer{ 0 },
        mDepthOffset{ depthOffset },
        mVisible{ true },
        mOpacity{ 1.0f }
    {
    }

    Bellota(Transform transform, TextureId textureId, MeshId meshId):
        mTransform( transform ),
        mTextureId{ textureId },
        mMeshId{ meshId },
        mCurrentLayer{ 0 },
        mDepthOffset{ 0 },
        mVisible{ true },
        mOpacity{ 1.0f }
    {
    }

    Bellota(Transform transform, TextureId textureId, MeshId meshId, std::int8_t depthOffset) :
        mTransform( transform ),
        mTextureId{ textureId },
        mMeshId{ meshId },
        mCurrentLayer{ 0 },
        mDepthOffset{ depthOffset },
        mVisible{ true },
        mOpacity{ 1.0f }
    {
    }

    /// Return a copy with the texture replaced. Every other field — transform,
    /// mesh id, current layer, depth offset, visible, opacity — is preserved.
    Bellota withTexture(TextureId textureId) const
    {
        Bellota copy = *this;
        copy.mTextureId = textureId;
        return copy;
    }

    /// Return a copy with the mesh replaced. Every other field is preserved.
    Bellota withMesh(MeshId meshId) const
    {
        Bellota copy = *this;
        copy.mMeshId = meshId;
        return copy;
    }

    const Transform& transform() const { return mTransform; }
    Transform& transform() { return mTransform; }

    const TextureId& texture() const { return mTextureId; }

    const std::optional<MeshId>& meshId() const { return mMeshId; }
    std::optional<MeshId>& meshId() { return mMeshId; }

    const std::int8_t& depthOffset() const { return mDepthOffset; }
    std::int8_t& depthOffset() { return mDepthOffset; }

    const bool& visible() const { return mVisible; }
    bool& visible() { return mVisible; }

    const float& opacity() const { return mOpacity; }
    float& opacity() { return mOpacity; }

    const std::size_t& currentLayer() const { return mCurrentLayer; }
    std::size_t& currentLayer() { return mCurrentLayer; }

private:
    Transform mTransform;
    TextureId mTextureId;
    std::optional<MeshId> mMeshId;    /**< nullopt only during the construction → addBellota window; always set inside the BellotaContainer. */
    std::size_t mCurrentLayer;        /**< The current layer being displayed */

    /* Greater values means closer to the viewer. Default is 0. Example, a background image could use depth offset -1. */
    std::int8_t mDepthOffset;

    /* You can hide this implementation */
    bool mVisible;

    float mOpacity;  /**< 0.0 = fully transparent, 1.0 = fully opaque (default). */
};

}
