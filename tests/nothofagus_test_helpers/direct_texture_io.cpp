#include "direct_texture_io.h"

#include <stb_image_plus.h>

#include <cstring>
#include <filesystem>
#include <span>
#include <stdexcept>

namespace Nothofagus::TestHelpers
{

namespace
{
// stb_image_plus::Pixel4 is std::array<uint8_t, 4> with no padding, so an RGBA
// byte buffer and a span of Pixel4 are layout-compatible.
static_assert(sizeof(stb_image_plus::Pixel4) == 4, "Pixel4 must be 4 tightly-packed bytes");
}

void save(const std::string& path, const DirectTexture& texture)
{
    TextureData data = texture.generateTextureData();
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
        throw std::runtime_error("Failed to write image: " + path);
}

DirectTexture load(const std::string& path)
{
    stb_image_plus::ImageData4 image(std::filesystem::path{path});
    if (!image.isValid())
        throw std::runtime_error("Failed to read image: " + path);

    const std::size_t width  = image.width();
    const std::size_t height = image.height();

    TextureData data(width, height, 1);
    std::span<std::uint8_t> dst = data.getDataSpan();
    std::span<stb_image_plus::Pixel4> src = image.pixelSpan();

    std::memcpy(dst.data(), src.data(), width * height * 4);

    return DirectTexture(std::move(data));
}

} // namespace Nothofagus::TestHelpers
