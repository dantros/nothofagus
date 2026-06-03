#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include "texture_id.h"
#include "mesh.h"

namespace Nothofagus
{

/* Visual description of a drawable element: which texture and mesh to draw, the
 * displayed layer, visibility and opacity. Deliberately free of any transform so
 * it can be reused on its own. */
class Visual
{
public:
    Visual(TextureId textureId):
        mTextureId{ textureId },
        mMeshId{ std::nullopt },
        mCurrentLayer{ 0 },
        mVisible{ true },
        mOpacity{ 1.0f }
    {
    }

    Visual(TextureId textureId, MeshId meshId):
        mTextureId{ textureId },
        mMeshId{ meshId },
        mCurrentLayer{ 0 },
        mVisible{ true },
        mOpacity{ 1.0f }
    {
    }

    /// Return a copy with the texture replaced. Every other field is preserved.
    Visual withTexture(TextureId textureId) const
    {
        Visual copy = *this;
        copy.mTextureId = textureId;
        return copy;
    }

    /// Return a copy with the mesh replaced. Every other field is preserved.
    Visual withMesh(MeshId meshId) const
    {
        Visual copy = *this;
        copy.mMeshId = meshId;
        return copy;
    }

    const TextureId& texture() const { return mTextureId; }

    const std::optional<MeshId>& meshId() const { return mMeshId; }
    std::optional<MeshId>& meshId() { return mMeshId; }

    const bool& visible() const { return mVisible; }
    bool& visible() { return mVisible; }

    const float& opacity() const { return mOpacity; }
    float& opacity() { return mOpacity; }

    const std::size_t& currentLayer() const { return mCurrentLayer; }
    std::size_t& currentLayer() { return mCurrentLayer; }

private:
    TextureId mTextureId;
    std::optional<MeshId> mMeshId;    /**< nullopt only during the construction → addBellota window; always set inside the BellotaContainer. */
    std::size_t mCurrentLayer;        /**< The current layer being displayed */

    /* You can hide this implementation */
    bool mVisible;

    float mOpacity;  /**< 0.0 = fully transparent, 1.0 = fully opaque (default). */
};

}
