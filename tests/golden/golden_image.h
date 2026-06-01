#pragma once

#include <texture.h>
#include <string>
#include <cstdint>
#include <cstddef>

namespace GoldenImage
{

/// Saves a DirectTexture's RGBA pixels as a PNG file (via stb_image_plus).
/// The path's extension selects the encoder; ".png" is the intended format.
/// Throws std::runtime_error if the image cannot be written.
void save(const std::string& path, const Nothofagus::DirectTexture& texture);

/// Loads an image file (PNG/BMP/TGA/JPG) into a DirectTexture as RGBA.
/// Throws std::runtime_error if the file cannot be read or decoded.
Nothofagus::DirectTexture load(const std::string& path);

/// Returns true if the file exists.
bool exists(const std::string& path);

/// Result of a tolerance-based comparison between two textures.
struct ComparisonResult
{
    bool         withinTolerance = false; ///< true when the images match within the given thresholds
    bool         sizeMismatch    = false; ///< true when dimensions differ (always a failure)
    std::size_t  differingPixels = 0;     ///< pixels with any channel delta exceeding perChannelTolerance
    std::uint8_t maxChannelDelta = 0;     ///< largest single-channel absolute difference observed
    double       meanChannelDelta = 0.0;  ///< mean absolute per-channel difference across the whole image
};

/// Compares two textures with a per-channel tolerance and a cap on how many
/// pixels may differ. A pixel counts as "differing" when any of its RGBA
/// channels differs by more than perChannelTolerance. The comparison is within
/// tolerance when sizes match and differingPixels <= maxDifferingPixels.
ComparisonResult compare(const Nothofagus::DirectTexture& actual,
                         const Nothofagus::DirectTexture& expected,
                         std::uint8_t perChannelTolerance,
                         std::size_t maxDifferingPixels);

/// Builds a visual diff texture: black where the two images agree, increasingly
/// red where they differ (the per-pixel max channel delta, amplified and clamped
/// so subtle differences are visible). Mismatched sizes produce a solid-red
/// texture sized to the larger of the two.
Nothofagus::DirectTexture makeDiff(const Nothofagus::DirectTexture& actual,
                                   const Nothofagus::DirectTexture& expected);

} // namespace GoldenImage
