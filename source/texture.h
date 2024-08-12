#pragma once

#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include "check.h"

namespace Nothofagus
{

struct Pixel
{
    std::uint8_t colorId;
};

struct ColorPallete
{
    std::vector<glm::vec4> colors;

    std::size_t size() const
    {
        return colors.size();
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
    Texture(const glm::ivec2 size, const glm::vec3 defaultColor):
        mSize(size),
        mPixels(size.x * size.y, 0),
    	mPallete{{defaultColor}}
    {
    }

    const std::vector<Pixel>& pixels() const
    {
        return mPixels;
    }

    void setPixels(const std::vector<Pixel>& pixels) const
    {
        debugCheck(mPixels.size() == pixels, "Invalid number of pixels for this texture.");
        debugCheck(std::any_of(pixels.cbegin(), pixels.cend(),
            [&mPallete](const Pixel& pixel)
            {
                return pixel.id >= mPallete.size();
            }
        ), "At least one of the pixels does not fit within the color pallete.");

        mPixels.swap(pixels);
    }

    const Pixel& pixel(const std::size_t i, const std::size_t j) const
    {
        const std::size_t index = indexOf(i, j);
        return mPixels.at(index);
    }

    void setPixel(const std::size_t i, const std::size_t j, const Pixel pixel)
    {
        debugCheck(pixel.colorId < mPallete.size(), "colorId is not present in color pallete.");
        const std::size_t index = indexOf(i, j);
        mPixels.at(index) = pixel;
    }

    const glm::vec3& color(const std::size_t i, const std::size_t j) const
    {
        const std::size_t index = indexOf(i, j);
        const Pixel& pixel = mPixels[index];
        return mPallete.colors.at(pixel.colorId);
    }

    const ColorPallete& pallete() const
    {
        return mPallete;
    }

    void setPallete(const ColorPallete& pallete)
    {
        assert(pallete.size() < mPallete.size());
        mPallete = pallete;
    }

private:
    glm::ivec2 mSize;
    std::vector<Pixel> mPixels;
    ColorPallete mPallete;
};

}