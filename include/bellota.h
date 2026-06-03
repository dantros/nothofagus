#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include "transform.h"
#include "mesh.h"
#include "texture_id.h"
#include "visual.h"

namespace Nothofagus
{

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
        mDepthOffset{ 0 },
        mVisual{ textureId }
    {
    }

    Bellota(Transform transform, TextureId textureId, std::int8_t depthOffset) :
        mTransform( transform ),
        mDepthOffset{ depthOffset },
        mVisual{ textureId }
    {
    }

    Bellota(Transform transform, TextureId textureId, MeshId meshId):
        mTransform( transform ),
        mDepthOffset{ 0 },
        mVisual{ textureId, meshId }
    {
    }

    Bellota(Transform transform, TextureId textureId, MeshId meshId, std::int8_t depthOffset) :
        mTransform( transform ),
        mDepthOffset{ depthOffset },
        mVisual{ textureId, meshId }
    {
    }

    /// Compose a bellota from an existing Visual (texture/mesh/layer/visible/opacity).
    Bellota(Transform transform, Visual visual, std::int8_t depthOffset = 0) :
        mTransform( transform ),
        mDepthOffset{ depthOffset },
        mVisual( visual )
    {
    }

    /// Return a copy with the texture replaced. Every other field — transform,
    /// mesh id, current layer, depth offset, visible, opacity — is preserved.
    Bellota withTexture(TextureId textureId) const
    {
        Bellota copy = *this;
        copy.mVisual = mVisual.withTexture(textureId);
        return copy;
    }

    /// Return a copy with the mesh replaced. Every other field is preserved.
    Bellota withMesh(MeshId meshId) const
    {
        Bellota copy = *this;
        copy.mVisual = mVisual.withMesh(meshId);
        return copy;
    }

    const Transform& transform() const { return mTransform; }
    Transform& transform() { return mTransform; }

    const Visual& visual() const { return mVisual; }
    Visual& visual() { return mVisual; }

    const TextureId& texture() const { return mVisual.texture(); }

    const std::optional<MeshId>& meshId() const { return mVisual.meshId(); }
    std::optional<MeshId>& meshId() { return mVisual.meshId(); }

    const std::int8_t& depthOffset() const { return mDepthOffset; }
    std::int8_t& depthOffset() { return mDepthOffset; }

    const bool& visible() const { return mVisual.visible(); }
    bool& visible() { return mVisual.visible(); }

    const float& opacity() const { return mVisual.opacity(); }
    float& opacity() { return mVisual.opacity(); }

    const std::size_t& currentLayer() const { return mVisual.currentLayer(); }
    std::size_t& currentLayer() { return mVisual.currentLayer(); }

private:
    Transform mTransform;

    /* Greater values means closer to the viewer. Default is 0. Example, a background image could use depth offset -1.
       Part of the transform group (z-ordering), not the Visual. */
    std::int8_t mDepthOffset;

    Visual mVisual;
};

}
