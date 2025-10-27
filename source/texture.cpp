#include "texture.h"
#include "check.h"
#include <algorithm>

namespace Nothofagus
{

std::size_t indexOf(const std::size_t sizeI, const std::size_t sizeJ, const std::size_t i, const std::size_t j)
{
    debugCheck(i < sizeI and j < sizeJ, "Invalid indices for this texture.");
    return sizeI * j + i;
}

std::size_t indexOf(const glm::ivec2 size, const std::size_t i, const std::size_t j)
{
    return indexOf(size.x, size.y, i, j);
}

IndirectTexture& IndirectTexture::setPixels(std::initializer_list<Pixel::ColorId> pixelColors, std::size_t layer)
{
    debugCheck(mPixels.size() / mLayers == pixelColors.size(), "Invalid number of pixels for this texture.");
    debugCheck(mLayers > layer, "Invalid layer.");
    const auto& pallete = mPallete;
    debugCheck(std::all_of(pixelColors.begin(), pixelColors.end(),
        [&pallete](const Pixel::ColorId colorId)
        {
            return static_cast<std::size_t>(colorId) <= pallete.size();
        }
    ), "At least one of the pixels does not fit within the color pallete.");

    //mPixels.assign(pixelColors.begin(), pixelColors.end());

    // Calcular el inicio y fin de los píxeles para este layer
    std::size_t startIdx = layer * mPixels.size() / mLayers;
    std::size_t endIdx = (layer + 1) * mPixels.size() / mLayers;

    // Actualizar solo los píxeles correspondientes al layer indicado
    auto pixelIt = pixelColors.begin();
    for (std::size_t idx = startIdx; idx < endIdx; ++idx, ++pixelIt)
    {
        const Pixel::ColorId& colorId = *pixelIt;
        mPixels[idx].colorId = colorId;  // Asignar el colorId desde el inicializador
    }

    return *this;
}

const Pixel& IndirectTexture::pixel(const std::size_t i, const std::size_t j, std::size_t layer) const
{
    debugCheck(mLayers > layer, "Invalid layer.");
    const std::size_t layerIndexStart = layer * mPixels.size() / mLayers;
    const std::size_t index = indexOf(mSize, i, j);
    return mPixels.at(layerIndexStart + index);
}

IndirectTexture& IndirectTexture::setPixel(const std::size_t i, const std::size_t j, const Pixel pixel, std::size_t layer)
{
    debugCheck(mLayers > layer, "Invalid layer.");
    debugCheck(pixel.colorId < mPallete.size(), "colorId is not present in color pallete.");
    std::size_t layerIndexStart = layer * mPixels.size() / mLayers;
    const std::size_t index = indexOf(mSize, i, j);
    mPixels.at(layerIndexStart + index) = pixel;
    return *this;

}

const glm::vec4& IndirectTexture::color(const std::size_t i, const std::size_t j, std::size_t layer) const
{
    debugCheck(mLayers > layer, "Invalid layer.");
    std::size_t layerIndexStart = layer * mPixels.size() / mLayers;
    const std::size_t index = indexOf(mSize, i, j);
    const Pixel& pixel = mPixels[layerIndexStart + index];
    return mPallete.colors.at(pixel.colorId);
}

IndirectTexture& IndirectTexture::setPallete(const ColorPallete& pallete)
{
    debugCheck(mPallete.size() <= pallete.size(), "");
    mPallete = pallete;
    return *this;
}

TextureData IndirectTexture::generateTextureData() const
{
    TextureData out(mSize.x, mSize.y, mLayers);
    std::span<std::uint8_t> dataSpan = out.getDataSpan();
    std::size_t dataSpanIndex = 0;

    for (std::size_t layer = 0; layer < mLayers; ++layer)
    {
        std::size_t layerIndexStart = layer * mPixels.size() / mLayers;
        for (std::size_t pixelIndex = 0; pixelIndex < out.width() * out.height(); ++pixelIndex)
        {
            const Pixel& pixel = mPixels[layerIndexStart + pixelIndex];
            const glm::vec4 color = 255.0f * mPallete.colors.at(pixel.colorId);
            dataSpan[dataSpanIndex    ] = color.r;
            dataSpan[dataSpanIndex + 1] = color.g;
            dataSpan[dataSpanIndex + 2] = color.b;
            dataSpan[dataSpanIndex + 3] = color.a;

            dataSpanIndex += 4;
        }
    }

    return out;
}

std::ostream& operator<<(std::ostream& os, const IndirectTexture& texture)
{
    for (std::size_t layer = 0; layer < texture.layers(); ++layer) 
    {
        for (int j= 0; j < texture.size().y; ++j)
        {
            for (int i = 0; i < texture.size().x; ++i)
            {
                os << static_cast<bool>(texture.pixel(i, j, layer).colorId) << " ";
            }
            os << std::endl;
        }
    }
    return os;
}

glm::ivec2 DirectTexture::size() const
{
    return {mTextureData.width(), mTextureData.height()};
}

void DirectTexture::setColor(const std::size_t i, const std::size_t j, const glm::vec4 &color)
{
    std::span<std::uint8_t> pixelSpan = mTextureData.getPixelSpan(i, j);
    debugCheck(pixelSpan.size() == 4);

    pixelSpan[0] = static_cast<std::uint8_t>(255.f * color.r);
    pixelSpan[1] = static_cast<std::uint8_t>(255.f * color.g);
    pixelSpan[2] = static_cast<std::uint8_t>(255.f * color.b);
    pixelSpan[3] = static_cast<std::uint8_t>(255.f * color.a);
}

glm::vec4 DirectTexture::color(const std::size_t i, const std::size_t j) const
{
    std::span<std::uint8_t> pixelSpan = mTextureData.getPixelSpan(i, j);
    debugCheck(pixelSpan.size() == 4);

    return {
        pixelSpan[0] / 255.f,
        pixelSpan[1] / 255.f,
        pixelSpan[2] / 255.f,
        pixelSpan[3] / 255.f
    };
}

TextureData DirectTexture::generateTextureData() const
{
    return mTextureData;
}

std::span<std::uint8_t> TextureData::getPixelSpan(const std::size_t i, const std::size_t j) const
{
    const std::size_t index = indexOf(mWidth, mHeight, i, j);
    const std::size_t offset = index * ColorDepth;
    std::uint8_t& firstPixelValue = mDataSpan[offset];
    std::uint8_t* firstPixelAddress = &firstPixelValue;
    return std::span<std::uint8_t>(firstPixelAddress, ColorDepth);
}

}