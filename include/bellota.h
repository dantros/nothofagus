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

struct TextureArrayId
{
    std::size_t id;
};

struct BellotaId
{
    std::size_t id;
};

struct AnimatedBellotaId
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
        mDepthOffset{ 0 },
        mVisible{ true }
        
    {
    }

    Bellota(Transform transform, TextureId textureId, std::int8_t depthOffset) :
        mTransform( transform ),
        mTextureId{ textureId },
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

private:
    Transform mTransform;
    TextureId mTextureId;

    /* Greater values means closer to the viewer. Default is 0. Example, a background image could use depth offset -1. */
    std::int8_t mDepthOffset;

    /* You can hide this implementation */
    bool mVisible;
};


/* Drawable element */
class AnimatedBellota
{
public:
    AnimatedBellota(Transform transform, TextureArrayId textureArrayId, size_t layers):
        mTransform( transform ),
        mTextureArrayId{ textureArrayId },
        mDepthOffset{ 0 },
        mLayers( layers ),
        mVisible{ true }
        
    {
    }

    AnimatedBellota(Transform transform, TextureArrayId textureArrayId, size_t layers, std::int8_t depthOffset) :
        mTransform( transform ),
        mTextureArrayId{ textureArrayId },
        mLayers( layers ),
        mDepthOffset{ depthOffset },
        mVisible{ true }

    {
    }

    const Transform& transform() const { return mTransform; }
    Transform& transform() { return mTransform; }

    const TextureArrayId& textureArray() const { return mTextureArrayId; }
    TextureArrayId& textureArray() { return mTextureArrayId; }

    const std::int8_t& depthOffset() const { return mDepthOffset; }
    std::int8_t& depthOffset() { return mDepthOffset; }

    const size_t& actualLayer() const { return mActualLayer; }
    size_t& actualLayer() { return mActualLayer; }

    const bool& visible() const { return mVisible; }
    bool& visible() { return mVisible; }

    void setActualLayer(const size_t layer) { mActualLayer = layer; } 


private:
    Transform mTransform;
    TextureArrayId mTextureArrayId;

    size_t mLayers;
    size_t mActualLayer = 0;
    /* Greater values means closer to the viewer. Default is 0. Example, a background image could use depth offset -1. */
    std::int8_t mDepthOffset;

    /* You can hide this implementation */
    bool mVisible;
};

};