#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <filesystem>

namespace stb_image_plus
{
template <std::size_t DesiredChannels>
struct PixelT
{
public:
    PixelT() = default;
    PixelT(std::initializer_list<std::uint8_t> values);
    const std::uint8_t& operator[](std::size_t coord) const;
    std::uint8_t& operator[](std::size_t coord);
    std::size_t channels() const;

private:
    std::array<std::uint8_t, DesiredChannels> mData;
};

template <std::size_t DesiredChannels>
class ImageData
{
public:
    using Pixel = PixelT<DesiredChannels>;

    ImageData();
    ImageData(const std::filesystem::path& filename);

    /* ImageData takes ownership of the memory pointed to by pixelSpan.
     * Number of pixels must be equal to width * height. */
    ImageData(std::span<Pixel> pixelSpan, std::size_t width, std::size_t height);
    
    /* Initializes the current invalid object by reading an image file. */
    bool read(const std::filesystem::path& filename);

    /* Initializes the current invalid object by decoding image data from memory. */
    bool readFromMemory(const std::uint8_t* data, std::size_t size);
    
    /* Encode and write to disk. Format is selected from the filename
     * extension (case-insensitive): .png, .bmp, .tga, .jpg, .jpeg.
     * Returns false for unsupported extensions or on encoder failure.
     *
     * `jpegQuality` controls JPEG output only (1..100, ignored for the
     * lossless formats). 90 is a reasonable default for photographic
     * content.
     *
     * Note: stb_image_write also provides HDR output, but that requires
     * float pixel data; ImageData<N> is uint8-based and therefore can't
     * round-trip HDR. Use the underlying stbi_write_hdr directly for
     * radiance maps. */
    bool write(const std::filesystem::path& filename, int jpegQuality = 90);
    bool isValid() const;
    std::span<Pixel> pixelSpan();
    std::span<const Pixel> pixelSpan() const;

    /* ImageData stops owning the data becoming invalid.
     * It returns the span for its usage outside this class, i.e. it will not be
     * deallocated on ImageData destruction. */
    std::span<Pixel> release();

    std::size_t width() const { return mWidth; }
    std::size_t height() const { return mHeight; }
    std::size_t internalChannels() const { return mInternalChannels; }
    std::size_t desiredChannels() const { return DesiredChannels; }
    const Pixel& at(std::size_t col, std::size_t row) const;
    Pixel& at(std::size_t col, std::size_t row);
    ImageData<DesiredChannels> resize(std::size_t width, std::size_t height);
    ImageData<DesiredChannels> resizeToWidth(std::size_t width);
    ImageData<DesiredChannels> resizeToHeight(std::size_t height);
    ~ImageData();

private:
    struct PixelContainer;
    std::unique_ptr<PixelContainer> mPixelsPtr;
    std::size_t mWidth, mHeight, mInternalChannels;
};

// Only these types are defined.
using Pixel1 = PixelT<1>;
using Pixel2 = PixelT<2>;
using Pixel3 = PixelT<3>;
using Pixel4 = PixelT<4>;

using ImageData1 = ImageData<1>;
using ImageData2 = ImageData<2>;
using ImageData3 = ImageData<3>;
using ImageData4 = ImageData<4>;

}