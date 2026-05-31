#include <stb_image_plus.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize2.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstddef>
#include <string>

void DebugCheck(bool condition)
{
    #ifdef _DEBUG
    if (not condition)
        throw;
    #endif
}

namespace stb_image_plus
{

template <std::size_t DesiredChannels>
PixelT<DesiredChannels>::PixelT(std::initializer_list<std::uint8_t> values)
{
    DebugCheck(values.size() == DesiredChannels);
    std::size_t index = 0;
    for (auto& value : values)
    {
        mData[index] = value;
        index++;
    }
}

template <std::size_t DesiredChannels>
const std::uint8_t& PixelT<DesiredChannels>::operator[](std::size_t coord) const
{
    DebugCheck(coord < DesiredChannels);
    return mData[coord];
}

template <std::size_t DesiredChannels>
std::uint8_t& PixelT<DesiredChannels>::operator[](std::size_t coord)
{
    DebugCheck(coord < DesiredChannels);
    return mData[coord];
}

template <std::size_t DesiredChannels>
std::size_t PixelT<DesiredChannels>::channels() const
{
    return DesiredChannels;
}

template <std::size_t DesiredChannels>
struct ImageData<DesiredChannels>::PixelContainer
{
    std::byte* data;
};

template <std::size_t DesiredChannels>
ImageData<DesiredChannels>::ImageData() :
    mPixelsPtr(std::make_unique<typename ImageData<DesiredChannels>::PixelContainer>()),
    mWidth(0),
    mHeight(0),
    mInternalChannels(0)
{
}

template <std::size_t DesiredChannels>
ImageData<DesiredChannels>::ImageData(const std::filesystem::path& filename) :
    mPixelsPtr(std::make_unique<typename ImageData<DesiredChannels>::PixelContainer>()),
    mWidth(0),
    mHeight(0),
    mInternalChannels(0)
{
    read(filename);
}

template <std::size_t DesiredChannels>
ImageData<DesiredChannels>::ImageData(std::span<Pixel> pixelSpan, std::size_t width, std::size_t height) :
    mPixelsPtr(std::make_unique<typename ImageData<DesiredChannels>::PixelContainer>()),
    mWidth(width),
    mHeight(height),
    mInternalChannels(0)
{
    DebugCheck(mPixelsPtr != nullptr);
    DebugCheck(pixelSpan.size() == width * height);
    auto firstPixelIt = pixelSpan.begin();
    Pixel& firstPixel = *firstPixelIt;
    Pixel* firstPixelAddress = &firstPixel;
    mPixelsPtr->data = reinterpret_cast<std::byte*>(firstPixelAddress);
    mInternalChannels = firstPixel.channels();
}

template <std::size_t DesiredChannels>
bool ImageData<DesiredChannels>::read(const std::filesystem::path& filename)
{
    DebugCheck(mPixelsPtr != nullptr);

    const std::u8string filenameAsUtf8 = filename.u8string();
    const char* filenameAsCharPtr = reinterpret_cast<const char*>(filenameAsUtf8.c_str());

    int width = 0, height = 0, internalChannels = 0;
    stbi_uc* imageDataPtr = stbi_load(filenameAsCharPtr, &width, &height, &internalChannels, DesiredChannels);
    mPixelsPtr->data = reinterpret_cast<std::byte*>(imageDataPtr);
    mWidth = static_cast<std::size_t>(width);
    mHeight = static_cast<std::size_t>(height);
    mInternalChannels = static_cast<std::size_t>(internalChannels);

    return mPixelsPtr->data != nullptr;
}

template <std::size_t DesiredChannels>
bool ImageData<DesiredChannels>::readFromMemory(const std::uint8_t* data, std::size_t size)
{
    DebugCheck(mPixelsPtr != nullptr);
    int width = 0, height = 0, internalChannels = 0;
    stbi_uc* imageDataPtr = stbi_load_from_memory(
        data, static_cast<int>(size), &width, &height, &internalChannels, DesiredChannels);
    mPixelsPtr->data = reinterpret_cast<std::byte*>(imageDataPtr);
    mWidth  = static_cast<std::size_t>(width);
    mHeight = static_cast<std::size_t>(height);
    mInternalChannels = static_cast<std::size_t>(internalChannels);
    return mPixelsPtr->data != nullptr;
}

template <std::size_t DesiredChannels>
bool ImageData<DesiredChannels>::write(const std::filesystem::path& filename, int jpegQuality)
{
    DebugCheck(mPixelsPtr != nullptr);
    const std::u8string filenameAsUtf8 = filename.u8string();
    const char* filenameAsCharPtr = reinterpret_cast<const char*>(filenameAsUtf8.c_str());

    std::string ext = filename.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Dispatch to the matching stb_image_write encoder. All formats here
    // operate on the uint8 pixel buffer the class owns; HDR (which needs
    // float input) is intentionally not included — see header comment.
    int result = 0;
    if (ext == ".png")
        result = stbi_write_png(filenameAsCharPtr, width(), height(),
                                DesiredChannels, mPixelsPtr->data,
                                width() * DesiredChannels);
    else if (ext == ".bmp")
        result = stbi_write_bmp(filenameAsCharPtr, width(), height(),
                                DesiredChannels, mPixelsPtr->data);
    else if (ext == ".tga")
        result = stbi_write_tga(filenameAsCharPtr, width(), height(),
                                DesiredChannels, mPixelsPtr->data);
    else if (ext == ".jpg" || ext == ".jpeg")
        result = stbi_write_jpg(filenameAsCharPtr, width(), height(),
                                DesiredChannels, mPixelsPtr->data,
                                jpegQuality);
    else
        return false;   // unsupported extension
    return result != 0;
}

template <std::size_t DesiredChannels>
bool ImageData<DesiredChannels>::isValid() const
{
    DebugCheck(mPixelsPtr != nullptr);
    return mPixelsPtr->data != nullptr;
}

template <std::size_t DesiredChannels>
std::span<typename ImageData<DesiredChannels>::Pixel> ImageData<DesiredChannels>::pixelSpan()
{
    DebugCheck(mPixelsPtr != nullptr);
    using Pixel = typename ImageData<DesiredChannels>::Pixel;
    Pixel* firstPixelPtr = reinterpret_cast<Pixel*>(mPixelsPtr->data);
    const std::size_t numberOfPixels = mWidth * mHeight;
    return std::span<Pixel>(firstPixelPtr, numberOfPixels);
}

template <std::size_t DesiredChannels>
std::span<const typename ImageData<DesiredChannels>::Pixel> ImageData<DesiredChannels>::pixelSpan() const
{
    DebugCheck(mPixelsPtr != nullptr);
    using Pixel = typename ImageData<DesiredChannels>::Pixel;
    const Pixel* firstPixelPtr = reinterpret_cast<const Pixel*>(mPixelsPtr->data);
    const std::size_t numberOfPixels = mWidth * mHeight;
    return std::span<const Pixel>(firstPixelPtr, numberOfPixels);
}

template <std::size_t DesiredChannels>
std::span<typename ImageData<DesiredChannels>::Pixel> ImageData<DesiredChannels>::release()
{
    DebugCheck(mPixelsPtr != nullptr);
    std::span<Pixel> out = pixelSpan();

    /* mPixelsPtr->data points to the image data, but mPixelsPtr does not own it.
     * the span object above keeps the references to the image data. 
     * mPixelsPtr needs to exist as an invariant. */
    mPixelsPtr->data = nullptr;

    mWidth = 0;
    mHeight = 0;
    mInternalChannels = 0;
    return out;
}

template <std::size_t DesiredChannels>
const typename ImageData<DesiredChannels>::Pixel& ImageData<DesiredChannels>::at(std::size_t col, std::size_t row) const
{
    DebugCheck(isValid());
    DebugCheck(col < mWidth);
    DebugCheck(row < mHeight);
    const std::size_t indexOffset = mWidth * row + col;
    using Pixel = typename ImageData<DesiredChannels>::Pixel;
    std::span<const Pixel> pixels = pixelSpan();
    return pixels[indexOffset];
}

template <std::size_t DesiredChannels>
typename ImageData<DesiredChannels>::Pixel& ImageData<DesiredChannels>::at(std::size_t col, std::size_t row)
{
    DebugCheck(isValid());
    DebugCheck(col < mWidth);
    DebugCheck(row < mHeight);
    const std::size_t indexOffset = mWidth * row + col;
    using Pixel = typename ImageData<DesiredChannels>::Pixel;
    std::span<Pixel> pixels = pixelSpan();
    return pixels[indexOffset];
}

template <std::size_t DesiredChannels>
ImageData<DesiredChannels> ImageData<DesiredChannels>::resize(std::size_t width, std::size_t height)
{
    // This may not work as expected for pixel layouts different than STBIR_1CHANNEL, STBIR_2CHANNEL, STBIR_RGB, STBIR_RGBA
    stbir_pixel_layout pixelLayout = static_cast<stbir_pixel_layout>(DesiredChannels);

    unsigned char* dataAsUCharPtr = reinterpret_cast<unsigned char*>(mPixelsPtr->data);

    unsigned char* dataAsUnsignedChars = stbir_resize_uint8_srgb(
        dataAsUCharPtr, mWidth, mHeight, DesiredChannels * mWidth,
        NULL, width, height, DesiredChannels * width, pixelLayout);

    using Pixel = typename ImageData<DesiredChannels>::Pixel;
    Pixel* dataAsPixels = reinterpret_cast<Pixel*>(dataAsUnsignedChars);
    std::span<Pixel> pixelSpan(dataAsPixels, width * height);
    return {pixelSpan, width, height};
}

template <std::size_t DesiredChannels>
ImageData<DesiredChannels> ImageData<DesiredChannels>::resizeToWidth(std::size_t width)
{
    std::size_t height = mHeight * width / mWidth;
    return resize(width, height);
}

template <std::size_t DesiredChannels>
ImageData<DesiredChannels> ImageData<DesiredChannels>::resizeToHeight(std::size_t height)
{
    std::size_t width = mWidth * height / mHeight;
    return resize(width, height);
}

template <std::size_t DesiredChannels>
ImageData<DesiredChannels>::~ImageData()
{
    DebugCheck(mPixelsPtr != nullptr);
    PixelContainer* pixels = mPixelsPtr.release();
    DebugCheck(pixels != nullptr);
    if (pixels->data == nullptr)
        return;
    stbi_image_free(pixels->data);
}

// template instantiations

template class ImageData<1>;
template class ImageData<2>;
template class ImageData<3>;
template class ImageData<4>;

template class PixelT<1>;
template class PixelT<2>;
template class PixelT<3>;
template class PixelT<4>;

}