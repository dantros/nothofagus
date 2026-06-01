#pragma once

#include <texture.h>
#include <string>

namespace Nothofagus::TestHelpers
{

/// Saves a DirectTexture's RGBA pixels as an image file (via stb_image_plus).
/// The path's extension selects the encoder; ".png" is the intended format.
/// Throws std::runtime_error if the image cannot be written.
void save(const std::string& path, const DirectTexture& texture);

/// Loads an image file (PNG/BMP/TGA/JPG) into a DirectTexture as RGBA.
/// Throws std::runtime_error if the file cannot be read or decoded.
DirectTexture load(const std::string& path);

} // namespace Nothofagus::TestHelpers
