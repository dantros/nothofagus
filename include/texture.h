#pragma once

#include <vector>
#include <initializer_list>
#include <glm/glm.hpp>
#include <iostream>

namespace Nothofagus
{

struct Pixel
{
    using ColorId = std::uint8_t;
    ColorId colorId;

    Pixel& operator=(const ColorId colorId_)
    {
        colorId = colorId_;
        return *this;
    }
};

struct ColorPallete
{
    std::vector<glm::vec4> colors;

    ColorPallete(std::size_t size, glm::vec4 colors_) : colors(size, colors_) {}

    ColorPallete(std::initializer_list<glm::vec4> colors_) : colors(colors_) {}

    ColorPallete& operator+=(float color)
    {
        for (auto& currentColor : colors)
        {
            currentColor += glm::vec4(color, color, color, 0.0);
        }

        return *this;
    }

    ColorPallete& operator*=(float color)
    {
        for (auto& currentColor : colors)
        {
            currentColor *= glm::vec4(color, color, color, 1.0);
        }

        return *this;
    }

    ColorPallete& operator+=(glm::vec3 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor += glm::vec4(color, 0.0);
        }

        return *this;
    }

    ColorPallete& operator*=(glm::vec3 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor *= glm::vec4(color, 1.0);
        }

        return *this;
    }

    ColorPallete& operator+=(glm::vec4 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor += color;
        }

        return *this;
    }

    ColorPallete& operator*=(glm::vec4 color)
    {
        for (auto& currentColor : colors)
        {
            currentColor *= color;
        }

        return *this;
    }

    std::size_t size() const
    {
        return colors.size();
    }
};

struct TextureData
{
    std::vector<std::uint8_t> data;
    std::size_t width, height;

    const std::uint8_t* getData() const
    {
        return data.data();
    }
};

/* Basic texture represented as a matrix with indices and a color pallete.
 * Each index is associated to a color in the pallete. */
class Texture
{
public:
    Texture(const glm::ivec2 size, const glm::vec4 defaultColor):
        mSize(size),
        mPixels(size.x* size.y, { 0 }),
        mPallete({ defaultColor })
    {
    }

    glm::ivec2 size() const { return mSize; }

    const std::vector<Pixel>& pixels() const { return mPixels; }

    void setPixels(std::initializer_list<Pixel::ColorId> pixelColors);

    template <typename PixelIt>
    void setPixels(const PixelIt& begin, const PixelIt& end)
    {
        mPixels.assign(begin, end);
    }

    const Pixel& pixel(const std::size_t i, const std::size_t j) const;

    Texture& setPixel(const std::size_t i, const std::size_t j, const Pixel pixel);

    const glm::vec4& color(const std::size_t i, const std::size_t j) const;

    const ColorPallete& pallete() const { return mPallete; }

    Texture& setPallete(const ColorPallete& pallete);

    TextureData generateTextureData() const;

private:
    glm::ivec2 mSize;
    std::vector<Pixel> mPixels;
    ColorPallete mPallete;
};

std::ostream& operator<<(std::ostream& os, const Texture& texture);


struct TextureArrayData
{
    std::vector<std::uint8_t> data;
    std::size_t width, height, layers;

    const std::uint8_t* getData() const
    {
        return data.data();
    }
};

class TextureArray
{
public:
    TextureArray(const glm::ivec2 size, const size_t layers, const std::vector<glm::vec4> defaultColors):
        mSize(size),
        mLayers(layers),
        mPixels(size.x* size.y* layers, { 0 })
    {
        // debugCheck(defaultColors.size() != layers, "Different amount of palletes and layers.");
        mPalletes.resize(layers);
        // for (size_t i = 0; i < defaultColors.size(); ++i) {
        //     mPalletes[i] = { defaultColors[i] };
        // }
    }

    glm::ivec2 size() const { return mSize; }
    
    size_t layers() const { return mLayers; }

    const std::vector<Pixel>& pixels() const { return mPixels; }

    void setPixelsInLayer(std::initializer_list<Pixel::ColorId> pixelColors, size_t layer);

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

    const Pixel& pixelInLayer(const std::size_t i, const std::size_t j, size_t layer) const;

    TextureArray& setPixelInLayer(const std::size_t i, const std::size_t j, const Pixel pixel, size_t layer);

    const glm::vec4& colorInLayer(const std::size_t i, const std::size_t j, const size_t layer) const;

    const ColorPallete& layerPallete(const size_t layer) const { return *mPalletes[layer]; }

    TextureArray& setLayerPallete(ColorPallete& pallete, const size_t layer);

    TextureArrayData generateTextureArrayData() const;

private:
    size_t mLayers;
    glm::ivec2 mSize;
    std::vector<Pixel> mPixels;
    std::vector<ColorPallete*> mPalletes;
};

std::ostream& operator<<(std::ostream& os, const TextureArray& texture);
}