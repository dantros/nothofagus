#pragma once

#include <cstddef>
#include <cstdint>
#include "transform.h"

namespace Nothofagus
{

struct TextureId
{
    std::size_t id;
};

struct BellotaId
{
    std::size_t id;
};

/* Drawable element */
class Bellota
{
public:
    Bellota(Transform transform, TextureId textureId):
        mTransform( transform ),
        mTextureId{ textureId },
        mCurrentLayer{ 0 },
        mDepthOffset{ 0 },
        mVisible{ true }
        
    {
    }

    Bellota(Transform transform, TextureId textureId, std::int8_t depthOffset) :
        mTransform( transform ),
        mTextureId{ textureId },
        mCurrentLayer{ 0 },
        mDepthOffset{ depthOffset },
        mVisible{ true }

    {
    }

    const Transform& transform() const { return mTransform; }
    Transform& transform() { return mTransform; }

    const TextureId& texture() const { return mTextureId; }
    TextureId& texture() { return mTextureId; }

    const std::int8_t& depthOffset() const { return mDepthOffset; }
    std::int8_t& depthOffset() { return mDepthOffset; }

    const bool& visible() const { return mVisible; }
    bool& visible() { return mVisible; }

    const std::size_t& currentLayer() const { return mCurrentLayer; }
    std::size_t& currentLayer() { return mCurrentLayer; }

private:
    Transform mTransform;
    TextureId mTextureId;
    std::size_t mCurrentLayer;        /**< The current layer being displayed */

    /* Greater values means closer to the viewer. Default is 0. Example, a background image could use depth offset -1. */
    std::int8_t mDepthOffset;

    /* You can hide this implementation */
    bool mVisible;
};

}