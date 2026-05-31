#include "golden_image.h"

#include <stb_image_plus.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <span>
#include <stdexcept>

namespace GoldenImage
{

namespace
{
// stb_image_plus::Pixel4 is std::array<uint8_t, 4> with no padding, so an RGBA
// byte buffer and a span of Pixel4 are layout-compatible.
static_assert(sizeof(stb_image_plus::Pixel4) == 4, "Pixel4 must be 4 tightly-packed bytes");

// Amplify subtle differences so they are visible in the diff texture.
constexpr int diffAmplification = 8;
}

void save(const std::string& path, const Nothofagus::DirectTexture& texture)
{
    Nothofagus::TextureData data = texture.generateTextureData();
    const std::size_t width  = data.width();
    const std::size_t height = data.height();
    std::span<std::uint8_t> pixels = data.getDataSpan();

    auto* asPixels = reinterpret_cast<stb_image_plus::Pixel4*>(pixels.data());
    stb_image_plus::ImageData4 image(
        std::span<stb_image_plus::Pixel4>(asPixels, width * height), width, height);

    const bool ok = image.write(path);

    // The ImageData destructor calls stbi_image_free on its buffer. We handed it
    // memory owned by `data` (a std::vector inside TextureData), so we must release
    // it first to avoid freeing memory stb did not allocate.
    image.release();

    if (!ok)
        throw std::runtime_error("Failed to write golden image: " + path);
}

Nothofagus::DirectTexture load(const std::string& path)
{
    stb_image_plus::ImageData4 image(std::filesystem::path{path});
    if (!image.isValid())
        throw std::runtime_error("Failed to read golden image: " + path);

    const std::size_t width  = image.width();
    const std::size_t height = image.height();

    Nothofagus::TextureData data(width, height, 1);
    std::span<std::uint8_t> dst = data.getDataSpan();
    std::span<stb_image_plus::Pixel4> src = image.pixelSpan();

    std::memcpy(dst.data(), src.data(), width * height * 4);

    return Nothofagus::DirectTexture(std::move(data));
}

bool exists(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

ComparisonResult compare(const Nothofagus::DirectTexture& actual,
                         const Nothofagus::DirectTexture& expected,
                         std::uint8_t perChannelTolerance,
                         std::size_t maxDifferingPixels)
{
    ComparisonResult result;

    if (actual.size() != expected.size())
    {
        result.sizeMismatch    = true;
        result.withinTolerance = false;
        return result;
    }

    Nothofagus::TextureData actualData   = actual.generateTextureData();
    Nothofagus::TextureData expectedData = expected.generateTextureData();
    std::span<std::uint8_t> a = actualData.getDataSpan();
    std::span<std::uint8_t> e = expectedData.getDataSpan();

    if (a.size() != e.size())
    {
        result.sizeMismatch    = true;
        result.withinTolerance = false;
        return result;
    }

    std::uint64_t deltaSum = 0;
    const std::size_t pixelCount = a.size() / 4;
    for (std::size_t p = 0; p < pixelCount; ++p)
    {
        bool pixelDiffers = false;
        for (std::size_t c = 0; c < 4; ++c)
        {
            const std::size_t i = p * 4 + c;
            const int delta = std::abs(static_cast<int>(a[i]) - static_cast<int>(e[i]));
            deltaSum += static_cast<std::uint64_t>(delta);
            result.maxChannelDelta = std::max(result.maxChannelDelta, static_cast<std::uint8_t>(delta));
            if (delta > static_cast<int>(perChannelTolerance))
                pixelDiffers = true;
        }
        if (pixelDiffers)
            ++result.differingPixels;
    }

    result.meanChannelDelta = a.empty() ? 0.0 : static_cast<double>(deltaSum) / static_cast<double>(a.size());
    result.withinTolerance  = result.differingPixels <= maxDifferingPixels;
    return result;
}

Nothofagus::DirectTexture makeDiff(const Nothofagus::DirectTexture& actual,
                                   const Nothofagus::DirectTexture& expected)
{
    const glm::ivec2 actualSize   = actual.size();
    const glm::ivec2 expectedSize = expected.size();

    if (actualSize != expectedSize)
    {
        // Sizes differ: produce a solid-red texture sized to the larger image so
        // the viewer has something meaningful to display.
        const glm::ivec2 size{std::max(actualSize.x, expectedSize.x),
                              std::max(actualSize.y, expectedSize.y)};
        Nothofagus::TextureData data(size.x, size.y, 1);
        std::span<std::uint8_t> px = data.getDataSpan();
        for (std::size_t p = 0; p < px.size() / 4; ++p)
        {
            px[p * 4 + 0] = 255;
            px[p * 4 + 3] = 255;
        }
        return Nothofagus::DirectTexture(std::move(data));
    }

    Nothofagus::TextureData actualData   = actual.generateTextureData();
    Nothofagus::TextureData expectedData = expected.generateTextureData();
    std::span<std::uint8_t> a = actualData.getDataSpan();
    std::span<std::uint8_t> e = expectedData.getDataSpan();

    Nothofagus::TextureData diff(actualSize.x, actualSize.y, 1);
    std::span<std::uint8_t> d = diff.getDataSpan();

    const std::size_t pixelCount = a.size() / 4;
    for (std::size_t p = 0; p < pixelCount; ++p)
    {
        int maxDelta = 0;
        for (std::size_t c = 0; c < 4; ++c)
        {
            const std::size_t i = p * 4 + c;
            maxDelta = std::max(maxDelta, std::abs(static_cast<int>(a[i]) - static_cast<int>(e[i])));
        }
        const int red = std::min(255, maxDelta * diffAmplification);
        d[p * 4 + 0] = static_cast<std::uint8_t>(red);
        d[p * 4 + 1] = 0;
        d[p * 4 + 2] = 0;
        d[p * 4 + 3] = 255;
    }

    return Nothofagus::DirectTexture(std::move(diff));
}

} // namespace GoldenImage
