#pragma once

#include <vector>
#include <initializer_list>
#include <glm/glm.hpp>
#include <iostream>

namespace Nothofagus
{

/**
 * @class Pixel
 * @brief Represents a single pixel in a texture.
 * 
 * The `Pixel` class is used to store the color index (as a `ColorId`) for a pixel in a texture.
 */
struct Pixel
{
    using ColorId = std::uint8_t; /**< Type for the color index (ID) of a pixel. */
    ColorId colorId; /**< The color index for this pixel. */

    /**
     * @brief Overloads the assignment operator to set the `colorId`.
     * 
     * This operator allows assigning a `ColorId` to the `Pixel` object.
     * 
     * @param colorId_ The color index to be assigned.
     * @return A reference to this `Pixel` object.
     */
    Pixel& operator=(const ColorId colorId_)
    {
        colorId = colorId_;
        return *this;
    }
};

/**
 * @class ColorPallete
 * @brief Represents a collection of colors (a palette) used in textures.
 * 
 * The `ColorPallete` class stores a list of colors, and supports several operations like 
 * adding, multiplying, or incrementing all the colors in the palette.
 */
struct ColorPallete
{
    std::vector<glm::vec4> colors; /**< A vector of RGBA colors. */

    /**
     * @brief Constructs a `ColorPallete` with a single color repeated.
     * 
     * @param size The number of colors in the palette.
     * @param colors_ The color to fill the palette with.
     */
    ColorPallete(std::size_t size, glm::vec4 colors_) : colors(size, colors_) {}

    /**
     * @brief Constructs a `ColorPallete` with the given list of colors.
     * 
     * @param colors_ An initializer list of RGBA colors.
     */
    ColorPallete(std::initializer_list<glm::vec4> colors_) : colors(colors_) {}

    /**
     * @brief Adds a scalar value to all colors in the palette.
     * 
     * This method adds the same scalar value to the R, G, and B components of each color in the palette.
     * 
     * @param color The value to be added.
     * @return A reference to the updated palette.
     */
    ColorPallete& operator+=(float color)
    {
        for (auto& currentColor : colors)
        {
            currentColor += glm::vec4(color, color, color, 0.0);
        }

        return *this;
    }

    /**
     * @brief Multiplies all colors in the palette by a scalar value.
     * 
     * This method multiplies the R, G, and B components of each color by the scalar value.
     * 
     * @param color The scalar value to multiply each color by.
     * @return A reference to the updated palette.
     */
    ColorPallete& operator*=(float color)
    {
        for (auto& currentColor : colors)
        {
            currentColor *= glm::vec4(color, color, color, 1.0);
        }

        return *this;
    }

    /**
     * @brief Adds a vector color to all colors in the palette.
     * 
     * This method adds the RGB components of the `glm::vec3` to the R, G, and B components of each color.
     * 
     * @param color The vector color to be added.
     * @return A reference to the updated palette.
     */
    ColorPallete& operator+=(glm::vec3 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor += glm::vec4(color, 0.0);
        }

        return *this;
    }

    /**
     * @brief Multiplies all colors in the palette by a vector color.
     * 
     * This method multiplies the R, G, and B components of each color by the corresponding components of the `glm::vec3`.
     * 
     * @param color The vector color to multiply each color by.
     * @return A reference to the updated palette.
     */
    ColorPallete& operator*=(glm::vec3 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor *= glm::vec4(color, 1.0);
        }

        return *this;
    }

    /**
     * @brief Adds a vector color (with alpha) to all colors in the palette.
     * 
     * This method adds a `glm::vec4` color (including alpha) to each color in the palette.
     * 
     * @param color The `glm::vec4` color to be added.
     * @return A reference to the updated palette.
     */
    ColorPallete& operator+=(glm::vec4 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor += color;
        }

        return *this;
    }

    /**
     * @brief Multiplies all colors in the palette by a vector color (with alpha).
     * 
     * This method multiplies the R, G, B, and A components of each color by the corresponding components of the `glm::vec4`.
     * 
     * @param color The `glm::vec4` color to multiply each color by.
     * @return A reference to the updated palette.
     */
    ColorPallete& operator*=(glm::vec4 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor *= color;
        }

        return *this;
    }

    /**
     * @brief Returns the number of colors in the palette.
     * 
     * @return The size of the `colors` vector.
     */
    std::size_t size() const
    {
        return colors.size();
    }
};

/**
 * @struct TextureData
 * @brief Represents the raw texture data including pixel colors.
 * 
 * This structure holds the raw texture data, width, and height, and provides a function to retrieve the data.
 */
struct TextureData
{
    std::vector<std::uint8_t> data; /**< Raw texture data in RGBA format. */
    std::size_t width, height; /**< The dimensions of the texture. */

    /**
     * @brief Returns a pointer to the raw texture data.
     * 
     * @return A pointer to the texture data.
     */
    const std::uint8_t* getData() const
    {
        return data.data();
    }
};

/**
 * @class Texture
 * @brief Represents a basic texture, including its pixel data and color palette.
 * 
 * This class holds a texture as a matrix of pixels, each associated with an index of a color 
 * from the color palette.
 */
class Texture
{
public:

    /**
     * @brief Constructs a texture with a specified size and a default color.
     * 
     * This constructor initializes the texture with the given size and a default color for all pixels.
     * 
     * @param size The size of the texture (width and height).
     * @param defaultColor The default color for all pixels.
     */
    Texture(const glm::ivec2 size, const glm::vec4 defaultColor):
        mSize(size),
        mPixels(size.x* size.y, { 0 }),
        mPallete({ defaultColor })
    {
    }

    /**
     * @brief Returns the size of the texture.
     * 
     * @return The size of the texture (width and height).
     */
    glm::ivec2 size() const { return mSize; }

    /**
     * @brief Returns a reference to the list of pixels in the texture.
     * 
     * @return A reference to the `std::vector<Pixel>` containing the pixel data.
     */
    const std::vector<Pixel>& pixels() const { return mPixels; }

    /**
     * @brief Sets the pixel colors using an initializer list of color IDs.
     * 
     * @param pixelColors The list of color IDs to assign to the pixels.
     */
    void setPixels(std::initializer_list<Pixel::ColorId> pixelColors);

    /**
     * @brief Sets the pixel colors using an iterator range.
     * 
     * @tparam PixelIt The iterator type.
     * @param begin The starting iterator.
     * @param end The ending iterator.
     */
    template <typename PixelIt>
    void setPixels(const PixelIt& begin, const PixelIt& end)
    {
        mPixels.assign(begin, end);
    }

    /**
     * @brief Returns the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @return The `Pixel` at the specified position.
     */
    const Pixel& pixel(const std::size_t i, const std::size_t j) const;

    /**
     * @brief Sets the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @param pixel The pixel to set at the specified position.
     * @return A reference to this texture.
     */
    Texture& setPixel(const std::size_t i, const std::size_t j, const Pixel pixel);

    /**
     * @brief Returns the color of the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @return The `glm::vec4` color of the pixel.
     */
    const glm::vec4& color(const std::size_t i, const std::size_t j) const;

    /**
     * @brief Returns the color palette of the texture.
     * 
     * @return A reference to the `ColorPallete` object.
     */
    const ColorPallete& pallete() const { return mPallete; }

    /**
     * @brief Sets the color palette of the texture.
     * 
     * @param pallete The new color palette to set.
     * @return A reference to this texture.
     */
    Texture& setPallete(const ColorPallete& pallete);

    /**
     * @brief Generates the raw texture data for the texture.
     * 
     * @return A `TextureData` object containing the raw texture data.
     */
    TextureData generateTextureData() const;

private:
    glm::ivec2 mSize; /**< The size of the texture (width and height). */
    std::vector<Pixel> mPixels; /**< The pixel data of the texture. */
    ColorPallete mPallete; /**< The color palette used by the texture. */
};

std::ostream& operator<<(std::ostream& os, const Texture& texture);


/**
 * @struct TextureArrayData
 * @brief Represents the raw texture data including pixel colors for every layer.
 * 
 * This structure holds the raw texture data of every layer, width, and height, and provides a function to retrieve the data.
 */
struct TextureArrayData
{
    std::vector<std::uint8_t> data;
    std::size_t width, height, layers;

    const std::uint8_t* getData() const
    {
        return data.data();
    }
};

/**
 * @class TextureArray
 * @brief Represents a texture array consisting of multiple layers of textures.
 * 
 * This class manages a texture array, which consists of several layers of textures. Each layer is a `Texture` object
 * and can be independently modified or displayed.
 */
class TextureArray
{
public:

    /**
     * @brief Constructs a texture array with the specified size and number of layers.
     * 
     * @param size The size of the textures in the array (width and height).
     * @param layers The number of layers in the texture array.
     */
    TextureArray(const glm::ivec2 size, const size_t layers):
        mSize(size),
        mLayers(layers),
        mPixels(size.x* size.y* layers, { 0 })
    {
        mPalletes.resize(layers);
        
    }

    /**
     * @brief Returns the size of the texture array.
     * 
     * @return The size of the texture array (width and height).
     */
    glm::ivec2 size() const { return mSize; }
    
    /**
     * @brief Returns the number of layers in the texture array.
     * 
     * @return The number of layers.
     */
    size_t layers() const { return mLayers; }

    /**
     * @brief Returns a reference to the list of pixels in the texture array.
     * 
     * @return A reference to the `std::vector<Pixel>` containing the pixel data of the texture array.
     */
    const std::vector<Pixel>& pixels() const { return mPixels; }

    /**
     * @brief Sets the pixel colors for a specific layer in the texture array.
     * 
     * @param pixelColors The list of color IDs to assign to the pixels in the specified layer.
     * @param layer The layer index to update.
     */
    void setPixelsInLayer(std::initializer_list<Pixel::ColorId> pixelColors, size_t layer);

    /**
     * @brief Sets the pixel colors for a specific layer in the texture array using iterators.
     * 
     * @tparam PixelIt The iterator type.
     * @param begin The starting iterator.
     * @param end The ending iterator.
     * @param layer The layer index to update.
     */
    template <typename PixelIt>
    void setPixelsInLayer(const PixelIt& begin, const PixelIt& end, size_t layer)
    {
        // debugCheck(mLayers > layer, "Invalid number of layer.");
        // Calcular el inicio y fin de los píxeles para este layer
        size_t startIdx = layer * mPixels.size() / mLayers;
        size_t endIdx = (layer + 1) * mPixels.size() / mLayers;

        // Actualizar solo los píxeles correspondientes al layer indicado
        auto pixelIt = begin;
        for (size_t idx = startIdx; idx < endIdx && pixelIt != end; ++idx, ++pixelIt)
        {
            mPixels[idx].colorId = static_cast<std::size_t>(pixelIt);  // Asignar el colorId desde los iteradores
        }
    }

    /**
     * @brief Returns the pixel at the specified position in a specific layer.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @param layer The layer index.
     * @return The `Pixel` at the specified position and layer.
     */
    const Pixel& pixelInLayer(const std::size_t i, const std::size_t j, size_t layer) const;

    /**
     * @brief Sets the pixel at the specified position in a specific layer.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @param pixel The pixel to set.
     * @param layer The layer index.
     * @return A reference to this `TextureArray`.
     */
    TextureArray& setPixelInLayer(const std::size_t i, const std::size_t j, const Pixel pixel, size_t layer);

    /**
     * @brief Returns the color of the pixel at the specified position in a specific layer.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @param layer The layer index.
     * @return The `glm::vec4` color of the pixel.
     */
    const glm::vec4& colorInLayer(const std::size_t i, const std::size_t j, const size_t layer) const;

    /**
     * @brief Returns the color palette of a specific layer.
     * 
     * @param layer The layer index.
     * @return A reference to the `ColorPallete` object of the specified layer.
     */
    const ColorPallete& layerPallete(const size_t layer) const { return *mPalletes[layer]; }

    /**
     * @brief Sets the color palette for a specific layer.
     * 
     * @param pallete The new color palette.
     * @param layer The layer index.
     * @return A reference to this `TextureArray`.
     */
    TextureArray& setLayerPallete(ColorPallete& pallete, const size_t layer);

    /**
     * @brief Generates the raw texture data for the entire texture array.
     * 
     * @return A `TextureArrayData` object containing the raw texture data.
     */
    TextureArrayData generateTextureArrayData() const;

private:
    size_t mLayers; /**< The number of layers in the texture array. */
    glm::ivec2 mSize; /**< The size of the textures (width and height). */
    std::vector<Pixel> mPixels; /**< The pixel data for all layers in the texture array. */
    std::vector<ColorPallete*> mPalletes; /**< The color palettes for each layer. */
};

/**
 * @brief Outputs the texture array data to a stream.
 * 
 * This operator allows streaming the `TextureArray` to an output stream, useful for debugging or logging.
 * 
 * @param os The output stream.
 * @param texture The `TextureArray` object.
 * @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const TextureArray& texture);
}