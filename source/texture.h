#pragma once

#include <vector>
#include <algorithm>
#include <initializer_list>
#include <glm/glm.hpp>
#include "check.h"

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

    ColorPallete(std::initializer_list<glm::vec4> colors_) : colors(colors_) {}

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
private:
    std::size_t indexOf(const std::size_t i, const std::size_t j) const
    {
        debugCheck(i < mSize.x and j < mSize.y, "Invalid indices for this texture.");
        return mSize.y * j + i;
    }

public:
    Texture(const glm::ivec2 size, const glm::vec4 defaultColor):
        mSize(size),
        mPixels(size.x* size.y, { 0 }),
        mPallete({ defaultColor })
    {
    }

    glm::ivec2 size() const
    {
        return mSize;
    }

    const std::vector<Pixel>& pixels() const
    {
        return mPixels;
    }

    void setPixels(std::initializer_list<Pixel::ColorId> pixelColors)
    {
        debugCheck(mPixels.size() == pixelColors.size(), "Invalid number of pixels for this texture.");
        const auto& pallete = mPallete;
        debugCheck(std::all_of(pixelColors.begin(), pixelColors.end(),
            [&pallete](const Pixel::ColorId colorId)
            {
                return static_cast<std::size_t>(colorId) <= pallete.size();
            }
        ), "At least one of the pixels does not fit within the color pallete.");

        mPixels.assign(pixelColors.begin(), pixelColors.end());
    }

    const Pixel& pixel(const std::size_t i, const std::size_t j) const
    {
        const std::size_t index = indexOf(i, j);
        return mPixels.at(index);
    }

    Texture& setPixel(const std::size_t i, const std::size_t j, const Pixel pixel)
    {
        debugCheck(pixel.colorId < mPallete.size(), "colorId is not present in color pallete.");
        const std::size_t index = indexOf(i, j);
        mPixels.at(index) = pixel;
        return *this;
    }

    const glm::vec4& color(const std::size_t i, const std::size_t j) const
    {
        const std::size_t index = indexOf(i, j);
        const Pixel& pixel = mPixels[index];
        return mPallete.colors.at(pixel.colorId);
    }

    const ColorPallete& pallete() const
    {
        return mPallete;
    }

    Texture& setPallete(const ColorPallete& pallete)
    {
        debugCheck(mPallete.size() <= pallete.size(), "");
        mPallete = pallete;
        return *this;
    }

    TextureData generateTextureData() const
    {
        TextureData out;
        out.width = mSize.x;
        out.height = mSize.y;
        unsigned int colorDepth = 4;

        out.data.reserve(out.width * out.height * colorDepth);

        for (std::size_t index = 0; index < out.width * out.height; ++index)
        {
            const Pixel& pixel = mPixels[index];
            const glm::vec4 color = 255.0f * mPallete.colors.at(pixel.colorId);
            out.data.push_back(color.r);
            out.data.push_back(color.g);
            out.data.push_back(color.b);
            out.data.push_back(color.a);
        }

        return out;
    }

private:
    glm::ivec2 mSize;
    std::vector<Pixel> mPixels;
    ColorPallete mPallete;
};

}