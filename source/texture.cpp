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
    TextureData out;
    out.layers = mLayers;
    out.width = mSize.x;
    out.height = mSize.y;
    static unsigned int colorDepth = 4;

    out.data.reserve(out.layers * out.width * out.height * colorDepth);

    for (std::size_t layer = 0; layer < mLayers; ++layer)
    {
        std::size_t layerIndexStart = layer * mPixels.size() / mLayers;
        for (std::size_t index = 0; index < out.width * out.height; ++index)
        {
            const Pixel& pixel = mPixels[layerIndexStart + index];
            const glm::vec4 color = 255.0f * mPallete.colors.at(pixel.colorId);
            out.data.push_back(color.r);
            out.data.push_back(color.g);
            out.data.push_back(color.b);
            out.data.push_back(color.a);
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

TextureData DirectTexture::generateTextureData() const
{
    TextureData out;
    out.width = mSize.x;
    out.height = mSize.y;
    static unsigned int colorDepth = 4;

    out.data.reserve(out.layers * out.width * out.height * colorDepth);

    for (std::size_t index = 0; index < out.width * out.height; ++index)
    {
        const glm::vec4 color = mPixels[index];
        out.data.push_back(color.r);
        out.data.push_back(color.g);
        out.data.push_back(color.b);
        out.data.push_back(color.a);
    }

    return out;
}

}