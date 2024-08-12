#pragma once

#include <cstddef>
#include "transform.h"

namespace Nothofagus
{

struct TextureId
{
    std::size_t id;
};

/* Drawable element */
class Bellota
{
public:
    Bellota(Transform transform, TextureId textureId):
        mTransform(transform),
        mTextureId{textureId}
    {
    }

    const Transform& transform() const
    {
        return mTransform;
    }

    Transform& transform()
    {
        return mTransform;
    }

    const TextureId& texture() const
    {
        return mTextureId;
    }

    TextureId& texture()
    {
        return mTextureId;
    }

private:
    Transform mTransform;
    TextureId mTextureId;
};

}