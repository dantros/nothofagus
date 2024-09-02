#include "texture.h"
#include "check.h"
#include <algorithm>

namespace Nothofagus
{

std::size_t indexOf(const glm::ivec2 size, const std::size_t i, const std::size_t j)
{
    debugCheck(i < size.x and j < size.y, "Invalid indices for this texture.");
    return size.x * j + i;
}

void Texture::setPixels(std::initializer_list<Pixel::ColorId> pixelColors)
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

const Pixel& Texture::pixel(const std::size_t i, const std::size_t j) const
{
    const std::size_t index = indexOf(mSize, i, j);
    return mPixels.at(index);
}

Texture& Texture::setPixel(const std::size_t i, const std::size_t j, const Pixel pixel)
{
    debugCheck(pixel.colorId < mPallete.size(), "colorId is not present in color pallete.");
    const std::size_t index = indexOf(mSize, i, j);
    mPixels.at(index) = pixel;
    return *this;
}

const glm::vec4& Texture::color(const std::size_t i, const std::size_t j) const
{
    const std::size_t index = indexOf(mSize, i, j);
    const Pixel& pixel = mPixels[index];
    return mPallete.colors.at(pixel.colorId);
}

Texture& Texture::setPallete(const ColorPallete& pallete)
{
    debugCheck(mPallete.size() <= pallete.size(), "");
    mPallete = pallete;
    return *this;
}

TextureData Texture::generateTextureData() const
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

std::ostream& operator<<(std::ostream& os, const Texture& texture)
{
    for (int j= 0; j < texture.size().y; ++j)
    {
        for (int i = 0; i < texture.size().x; ++i)
        {
            os << static_cast<bool>(texture.pixel(i,j).colorId) << " ";
        }
        os << std::endl;
    }
    return os;
}

}