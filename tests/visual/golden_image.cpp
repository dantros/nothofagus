#include "golden_image.h"
#include <fstream>
#include <stdexcept>
#include <vector>

namespace GoldenImage
{

void save(const std::string& path, const Nothofagus::DirectTexture& texture)
{
    Nothofagus::TextureData data = texture.generateTextureData();
    const uint32_t width  = static_cast<uint32_t>(data.width());
    const uint32_t height = static_cast<uint32_t>(data.height());
    std::span<std::uint8_t> pixels = data.getDataSpan();

    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open golden file for writing: " + path);

    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));
    file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
}

Nothofagus::DirectTexture load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open golden file for reading: " + path);

    uint32_t width = 0, height = 0;
    file.read(reinterpret_cast<char*>(&width), sizeof(width));
    file.read(reinterpret_cast<char*>(&height), sizeof(height));

    Nothofagus::TextureData data(width, height, 1);
    std::span<std::uint8_t> pixels = data.getDataSpan();
    file.read(reinterpret_cast<char*>(pixels.data()), pixels.size());

    if (!file)
        throw std::runtime_error("Failed to read golden file (truncated?): " + path);

    return Nothofagus::DirectTexture(std::move(data));
}

bool exists(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

bool compare(const Nothofagus::DirectTexture& actual, const Nothofagus::DirectTexture& expected)
{
    if (actual.size() != expected.size())
        return false;

    Nothofagus::TextureData actualData   = actual.generateTextureData();
    Nothofagus::TextureData expectedData = expected.generateTextureData();
    std::span<std::uint8_t> actualPixels   = actualData.getDataSpan();
    std::span<std::uint8_t> expectedPixels = expectedData.getDataSpan();

    if (actualPixels.size() != expectedPixels.size())
        return false;

    for (std::size_t i = 0; i < actualPixels.size(); ++i)
    {
        if (actualPixels[i] != expectedPixels[i])
            return false;
    }
    return true;
}

} // namespace GoldenImage
