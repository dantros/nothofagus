#include "direct_texture_compare.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace Nothofagus::TestHelpers
{

namespace
{
// Amplify subtle differences so they are visible in the diff texture.
constexpr int diffAmplification = 8;
}

ComparisonResult compare(const DirectTexture& actual,
                         const DirectTexture& expected,
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

    TextureData actualData   = actual.generateTextureData();
    TextureData expectedData = expected.generateTextureData();
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

DirectTexture makeDiff(const DirectTexture& actual, const DirectTexture& expected)
{
    const glm::ivec2 actualSize   = actual.size();
    const glm::ivec2 expectedSize = expected.size();

    if (actualSize != expectedSize)
    {
        // Sizes differ: produce a solid-red texture sized to the larger image so
        // the viewer has something meaningful to display.
        const glm::ivec2 size{std::max(actualSize.x, expectedSize.x),
                              std::max(actualSize.y, expectedSize.y)};
        TextureData data(size.x, size.y, 1);
        std::span<std::uint8_t> px = data.getDataSpan();
        for (std::size_t p = 0; p < px.size() / 4; ++p)
        {
            px[p * 4 + 0] = 255;
            px[p * 4 + 3] = 255;
        }
        return DirectTexture(std::move(data));
    }

    TextureData actualData   = actual.generateTextureData();
    TextureData expectedData = expected.generateTextureData();
    std::span<std::uint8_t> a = actualData.getDataSpan();
    std::span<std::uint8_t> e = expectedData.getDataSpan();

    TextureData diff(actualSize.x, actualSize.y, 1);
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

    return DirectTexture(std::move(diff));
}

} // namespace Nothofagus::TestHelpers
