#pragma once

#include <vector>
#include <initializer_list>
#include <glm/glm.hpp>
#include <iostream>
#include <variant>
#include <optional>
#include <span>

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
 * @brief Represents the raw texture data including pixel colors for every layer.
 * 
 * This structure holds the raw texture data of every layer, width, and height, and provides a function to retrieve the data.
 */
class TextureData
{
public:
    static constexpr unsigned int colorDepth = 4;

    /* Constructor that will use memory within this object */
    TextureData(std::size_t width, std::size_t height, std::size_t layers = 1):
        mDataOpt(std::in_place, colorDepth * width * height * layers, 0),
        mDataSpan(mDataOpt.value().begin(), mDataOpt.value().end()),
        mWidth(width),
        mHeight(height),
        mLayers(layers)
    {
    }

    /* Constructor that will use external memory via the span provided */
    TextureData(std::span<std::uint8_t> dataSpan, std::size_t width, std::size_t height, std::size_t layers = 1):
        mDataOpt(std::nullopt),
        mDataSpan(dataSpan),
        mWidth(width),
        mHeight(height),
        mLayers(layers)
    {
    }

    std::size_t width() const { return mWidth; }
    std::size_t height() const { return mHeight; }
    std::size_t layers() const { return mLayers; }
    std::span<std::uint8_t> getDataSpan() const { return mDataSpan; }

private:
    std::optional<std::vector<std::uint8_t>> mDataOpt;
    std::span<std::uint8_t> mDataSpan;
    std::size_t mWidth, mHeight, mLayers = 1; /**< The dimensions of the texture. */
};

/**
 * @class Texture
 * @brief Represents a basic texture, including its pixel data and color palette.
 * 
 * This class holds a texture as a matrix of pixels, each associated with an index of a color 
 * from the color palette.
 */
class IndirectTexture
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
    IndirectTexture(const glm::ivec2 size, const glm::vec4 defaultColor, const std::size_t layers = 1):
        mLayers(layers),
        mSize(size),
        mPixels(size.x * size.y * mLayers, { 0 }),
        mPallete({ defaultColor })
    {
        //debugCheck(layers > 0);
    }

    /**
     * @brief Returns the number of layers in the texture.
     * 
     * @return The number of layers.
     */
    size_t layers() const { return mLayers; }

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
    IndirectTexture& setPixels(std::initializer_list<Pixel::ColorId> pixelColors, std::size_t layer = 0);

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
     * @param layer The layer of interest for this query
     * @return The `Pixel` at the specified position.
     */
    const Pixel& pixel(const std::size_t i, const std::size_t j, std::size_t layer = 0) const;

    /**
     * @brief Sets the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @param pixel The pixel to set at the specified position.
     * @return A reference to this texture.
     */
    IndirectTexture& setPixel(const std::size_t i, const std::size_t j, const Pixel pixel, std::size_t layer = 0);

    /**
     * @brief Returns the color of the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @return The `glm::vec4` color of the pixel.
     */
    const glm::vec4& color(const std::size_t i, const std::size_t j, std::size_t layer = 0) const;

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
    IndirectTexture& setPallete(const ColorPallete& pallete);

    /**
     * @brief Generates the raw texture data for the texture.
     * 
     * @return A `TextureData` object containing the raw texture data.
     */
    TextureData generateTextureData() const;

private:
    std::size_t mLayers; /**< The number of layers in the texture. */
    glm::ivec2 mSize; /**< The size of the texture (width and height). */
    std::vector<Pixel> mPixels; /**< The pixel data of the texture. */
    ColorPallete mPallete; /**< The color palette used by the texture. */
};

std::ostream& operator<<(std::ostream& os, const IndirectTexture& texture);

class DirectTexture
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
    DirectTexture(const glm::ivec2 size, const glm::vec4 defaultColor):
        mSize(size),
        mPixels(size.x * size.y, defaultColor)
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
    const std::vector<glm::vec4>& pixels() const { return mPixels; }

    /**
     * @brief Returns the color of the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @return The `glm::vec4` color of the pixel.
     */
    glm::vec4& color(const std::size_t i, const std::size_t j);

    /**
     * @brief Returns the color of the pixel at the specified position.
     * 
     * @param i The x-coordinate of the pixel.
     * @param j The y-coordinate of the pixel.
     * @return The `glm::vec4` color of the pixel.
     */
    const glm::vec4& color(const std::size_t i, const std::size_t j) const;

    /**
     * @brief Generates the raw texture data for the texture.
     * 
     * @return A `TextureData` object containing the raw texture data.
     */
    TextureData generateTextureData() const;

private:
    glm::ivec2 mSize; /**< The size of the texture (width and height). */
    std::vector<glm::vec4> mPixels; /**< The color data of the texture. */
};

std::ostream& operator<<(std::ostream& os, const DirectTexture& texture);

using Texture = std::variant<IndirectTexture, DirectTexture>;

struct GetTextureSizeVisitor
{
    glm::ivec2 operator()(const IndirectTexture& texture) const { return texture.size(); };
    glm::ivec2 operator()(const DirectTexture& texture) const { return texture.size(); };
};

struct GenerateTextureDataVisitor
{
    TextureData operator()(const IndirectTexture& texture) const { return texture.generateTextureData(); };
    TextureData operator()(const DirectTexture& texture) const { return texture.generateTextureData(); };
};

}