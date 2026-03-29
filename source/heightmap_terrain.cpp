#include "../include/heightmap_terrain.h"

namespace Nothofagus
{

HeightmapTerrain::HeightmapTerrain(
    std::vector<float> heights,
    std::size_t        rows,
    std::size_t        columns,
    float              worldWidth,
    float              worldDepth,
    float              maximumHeight,
    TextureId          colorTextureId)
    : mHeights{std::move(heights)}
    , mRows{rows}
    , mColumns{columns}
    , mWorldWidth{worldWidth}
    , mWorldDepth{worldDepth}
    , mMaximumHeight{maximumHeight}
    , mColorTextureId{colorTextureId}
{
}

std::vector<float>&       HeightmapTerrain::heights()       { return mHeights; }
const std::vector<float>& HeightmapTerrain::heights() const { return mHeights; }

std::size_t&       HeightmapTerrain::rows()       { return mRows; }
const std::size_t& HeightmapTerrain::rows() const { return mRows; }

std::size_t&       HeightmapTerrain::columns()       { return mColumns; }
const std::size_t& HeightmapTerrain::columns() const { return mColumns; }

float&       HeightmapTerrain::worldWidth()       { return mWorldWidth; }
const float& HeightmapTerrain::worldWidth() const { return mWorldWidth; }

float&       HeightmapTerrain::worldDepth()       { return mWorldDepth; }
const float& HeightmapTerrain::worldDepth() const { return mWorldDepth; }

float&       HeightmapTerrain::maximumHeight()       { return mMaximumHeight; }
const float& HeightmapTerrain::maximumHeight() const { return mMaximumHeight; }

TextureId&       HeightmapTerrain::colorTextureId()       { return mColorTextureId; }
const TextureId& HeightmapTerrain::colorTextureId() const { return mColorTextureId; }

} // namespace Nothofagus
