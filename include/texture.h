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

}