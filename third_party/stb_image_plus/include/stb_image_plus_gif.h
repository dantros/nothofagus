#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include <filesystem>

namespace stb_image_plus
{

struct GifData
{
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t frameCount = 0;
    std::size_t channels = 0;
    std::vector<int> frameDelays; // per-frame delay in ms

    bool loadFromMemory(const std::uint8_t* data, std::size_t size, int requestedChannels = 4);
    bool loadFromFile(const std::filesystem::path& filename);
    bool isValid() const;
    std::span<const std::uint8_t> framePixels(std::size_t frameIndex) const;

private:
    struct PixelDeleter { void operator()(void* p) const; };
    std::unique_ptr<std::uint8_t, PixelDeleter> mPixels;
};

}
