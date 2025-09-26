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


/**
 * @class AnimatedBellota
 * @brief Represents an animated object that can cycle through layers of a texture array.
 *
 * The `AnimatedBellota` class handles an animated object that uses multiple layers of textures. Each layer
 * corresponds to a different frame of the animation, and the class manages the current layer to be displayed.
 * It also handles the transformation and visibility of the animated object.
 */
class AnimatedBellota
{
public:

    /**
     * @brief Constructs an AnimatedBellota with specified transform, texture array, and layers.
     * 
     * This constructor initializes the `AnimatedBellota` with a `Transform`, a `TextureId` to link the
     * texture array, the number of layers, and sets the default values for depth offset and visibility.
     * 
     * @param transform The transformation to apply to the object (position, scale, etc.).
     * @param textureId The ID of the texture array to be used by the object.
     * @param layers The number of layers in the texture array.
     */
    AnimatedBellota(Transform transform, TextureId textureId, size_t layers):
        mTransform( transform ),
        mTextureId{ textureId },
        mDepthOffset{ 0 },
        mLayers( layers ),
        mVisible{ true }
        
    {
    }

    /**
     * @brief Constructs an AnimatedBellota with specified transform, texture array, layers, and depth offset.
     * 
     * This constructor is similar to the previous one, but it allows specifying a custom depth offset.
     * 
     * @param transform The transformation to apply to the object (position, scale, etc.).
     * @param textureId The ID of the texture array to be used by the object.
     * @param layers The number of layers in the texture array.
     * @param depthOffset The depth offset for the object, where higher values bring the object closer to the viewer.
     */
    AnimatedBellota(Transform transform, TextureId textureId, size_t layers, std::int8_t depthOffset) :
        mTransform( transform ),
        mTextureId{ textureId },
        mLayers( layers ),
        mDepthOffset{ depthOffset },
        mVisible{ true }

    {
    }

    /**
     * @brief Returns the transformation applied to the object.
     * 
     * @return A constant reference to the `Transform` object representing the transformation.
     */
    const Transform& transform() const { return mTransform; }

    /**
     * @brief Returns a reference to the transformation applied to the object.
     * 
     * This function allows modification of the object's transformation (position, rotation, scale, etc.).
     * 
     * @return A reference to the `Transform` object representing the transformation.
     */
    Transform& transform() { return mTransform; }

    /**
     * @brief Returns a reference to the transformation applied to the object.
     * 
     * This function allows modification of the object's transformation (position, rotation, scale, etc.).
     * 
     * @return A reference to the `Transform` object representing the transformation.
     */
    const TextureId& textureArray() const { return mTextureId; }

    /**
     * @brief Returns a reference to the ID of the texture array used by the object.
     * 
     * This function allows modification of the texture array ID, which could be used to switch texture arrays.
     * 
     * @return A reference to the `TextureId` representing the texture array ID.
     */
    TextureId& textureArray() { return mTextureId; }

    /**
     * @brief Returns the depth offset value for the object.
     * 
     * Depth offset determines how close or far the object is in the scene. Higher values bring the object closer
     * to the viewer. The default value is 0, with negative values pushing the object further away.
     * 
     * @return A constant reference to the `std::int8_t` depth offset.
     */
    const std::int8_t& depthOffset() const { return mDepthOffset; }

    /**
     * @brief Returns a reference to the depth offset for the object.
     * 
     * @return A reference to the `std::int8_t` depth offset.
     */
    std::int8_t& depthOffset() { return mDepthOffset; }

    /**
     * @brief Returns the current layer index being displayed.
     * 
     * This function returns the index of the current layer in the texture array that is being shown.
     * 
     * @return A constant reference to the current layer index (`size_t`).
     */
    const size_t& actualLayer() const { return mActualLayer; }

    /**
     * @brief Returns a reference to the current layer index.
     * 
     * This function allows modification of the current layer index.
     * 
     * @return A reference to the current layer index (`size_t`).
     */
    size_t& actualLayer() { return mActualLayer; }

    /**
     * @brief Returns whether the object is visible or not.
     * 
     * @return A constant reference to a boolean indicating the visibility of the object.
     */
    const bool& visible() const { return mVisible; }

    /**
     * @brief Returns a reference to the visibility flag of the object.
     * 
     * This function allows modification of the object's visibility.
     * 
     * @return A reference to the boolean flag controlling visibility.
     */
    bool& visible() { return mVisible; }

    /**
     * @brief Sets the current layer index to the specified layer.
     * 
     * This method allows updating the current layer to be displayed from the texture array.
     * 
     * @param layer The index of the layer to be displayed.
     */
    void setActualLayer(const size_t layer) { mActualLayer = layer; } 


private:
    Transform mTransform;           /**< The transformation applied to the object (position, scale, rotation) */
    TextureId mTextureId; /**< The ID of the texture array used by the object */
    size_t mLayers;                 /**< The number of layers in the texture array */
    size_t mActualLayer = 0;        /**< The current layer being displayed */
    std::int8_t mDepthOffset;       /**< The depth offset (higher values bring the object closer) */
    bool mVisible;                  /**< Flag indicating whether the object is visible */
};

};