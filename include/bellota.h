#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include "transform.h"
#include "transform3d.h"

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

/* Drawable element — can be screen-space (2D) or world-space (3D billboard).
   The variant type determines the rendering mode:
     std::variant holds Transform  → 2D screen-space rendering
     std::variant holds Transform3D → 3D world-space billboard rendering */
class Bellota
{
public:
    /* 2D screen-space constructors */
    Bellota(Transform transform, TextureId textureId) :
        mTransform(transform),
        mTextureId{ textureId },
        mCurrentLayer{ 0 },
        mDepthOffset{ 0 },
        mVisible{ true },
        mOpacity{ 1.0f }
    {}

    Bellota(Transform transform, TextureId textureId, std::int8_t depthOffset) :
        mTransform(transform),
        mTextureId{ textureId },
        mCurrentLayer{ 0 },
        mDepthOffset{ depthOffset },
        mVisible{ true },
        mOpacity{ 1.0f }
    {}

    /* 3D world-space constructor */
    Bellota(Transform3D transform3d, TextureId textureId) :
        mTransform(transform3d),
        mTextureId{ textureId },
        mCurrentLayer{ 0 },
        mDepthOffset{ 0 },
        mVisible{ true },
        mOpacity{ 1.0f }
    {}

    /* Returns true when this bellota is rendered in 3D world space. */
    bool isWorldSpace() const { return std::holds_alternative<Transform3D>(mTransform); }

    /* 2D transform accessor — debugCheck fires if this is a world-space bellota. */
    const Transform& transform() const;
    Transform& transform();

    /* 3D transform accessor — debugCheck fires if this is a screen-space bellota. */
    const Transform3D& transform3d() const;
    Transform3D& transform3d();

    /* Texture accessor — const and mutable (mutable needed by setTexture copy-swap). */
    const TextureId& texture() const { return mTextureId; }
    TextureId& texture() { return mTextureId; }

    const std::int8_t& depthOffset() const { return mDepthOffset; }
    std::int8_t& depthOffset() { return mDepthOffset; }

    const bool& visible() const { return mVisible; }
    bool& visible() { return mVisible; }

    const float& opacity() const { return mOpacity; }
    float& opacity() { return mOpacity; }

    const std::size_t& currentLayer() const { return mCurrentLayer; }
    std::size_t& currentLayer() { return mCurrentLayer; }

private:
    std::variant<Transform, Transform3D> mTransform;
    TextureId mTextureId;
    std::size_t mCurrentLayer;        /**< The current layer being displayed */

    /* Greater values means closer to the viewer. Default is 0. Only used for 2D (screen-space) bellotas. */
    std::int8_t mDepthOffset;

    bool mVisible;

    float mOpacity;  /**< 0.0 = fully transparent, 1.0 = fully opaque (default). */
};

}
