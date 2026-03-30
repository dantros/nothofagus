#pragma once

#include <texture.h>
#include <string>
#include <cstdint>
#include <span>

namespace GoldenImage
{

/// Saves a DirectTexture's RGBA data as a golden binary file.
/// Format: uint32_t width, uint32_t height, then width*height*4 bytes of RGBA.
void save(const std::string& path, const Nothofagus::DirectTexture& texture);

/// Loads a golden binary file into a DirectTexture.
/// Throws std::runtime_error if the file cannot be read.
Nothofagus::DirectTexture load(const std::string& path);

/// Returns true if the file exists.
bool exists(const std::string& path);

/// Compares two DirectTextures pixel-by-pixel. Returns true if identical.
bool compare(const Nothofagus::DirectTexture& actual, const Nothofagus::DirectTexture& expected);

} // namespace GoldenImage
