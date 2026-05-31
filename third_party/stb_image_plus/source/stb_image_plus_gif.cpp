#include <stb_image_plus_gif.h>
#include <stb_image.h>
#include <fstream>
#include <cstring>

namespace stb_image_plus
{

void GifData::PixelDeleter::operator()(void* p) const
{
    stbi_image_free(p);
}

bool GifData::loadFromMemory(const std::uint8_t* data, std::size_t size, int requestedChannels)
{
    int* delays = nullptr;
    int x = 0, y = 0, z = 0, comp = 0;

    stbi_uc* result = stbi_load_gif_from_memory(
        data, static_cast<int>(size),
        &delays, &x, &y, &z, &comp, requestedChannels);

    if (!result)
        return false;

    mPixels.reset(result);
    width      = static_cast<std::size_t>(x);
    height     = static_cast<std::size_t>(y);
    frameCount = static_cast<std::size_t>(z);
    channels   = (requestedChannels > 0) ? static_cast<std::size_t>(requestedChannels)
                                         : static_cast<std::size_t>(comp);

    frameDelays.resize(frameCount);
    if (delays)
    {
        std::memcpy(frameDelays.data(), delays, frameCount * sizeof(int));
        stbi_image_free(delays);
    }

    return true;
}

bool GifData::loadFromFile(const std::filesystem::path& filename)
{
    const std::u8string filenameAsUtf8 = filename.u8string();
    const char* filenameAsCharPtr = reinterpret_cast<const char*>(filenameAsUtf8.c_str());

    std::ifstream file(filenameAsCharPtr, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
        return false;

    return loadFromMemory(buffer.data(), buffer.size());
}

bool GifData::isValid() const
{
    return mPixels != nullptr && frameCount > 0;
}

std::span<const std::uint8_t> GifData::framePixels(std::size_t frameIndex) const
{
    const std::size_t bytesPerFrame = width * height * channels;
    const std::uint8_t* frameStart = mPixels.get() + frameIndex * bytesPerFrame;
    return { frameStart, bytesPerFrame };
}

}
