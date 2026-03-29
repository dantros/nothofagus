#pragma once

#include "bellota.h"  // for TextureId
#include <vector>
#include <cstddef>

namespace Nothofagus
{

struct HeightmapTerrainId
{
    std::size_t id;

    bool operator==(const HeightmapTerrainId& rhs) const { return id == rhs.id; }
};

/**
 * @class HeightmapTerrain
 * @brief A 3D terrain surface defined by a grid of height values.
 *
 * Heights are provided as normalized floats (0.0 = lowest, 1.0 = highest).
 * The mesh is laid out on the XZ plane; Y is elevation (up).
 * Heights are baked into the mesh CPU-side on first use.
 *
 * Layout of mHeights: row-major, mHeights[row * mColumns + col].
 * X runs from 0 to mWorldWidth across columns.
 * Z runs from 0 to mWorldDepth across rows.
 * Y = height * mMaximumHeight.
 *
 * The terrain colour is provided by an existing TextureId (reuses the texture system).
 */
class HeightmapTerrain
{
public:
    HeightmapTerrain(
        std::vector<float> heights,
        std::size_t        rows,
        std::size_t        columns,
        float              worldWidth,
        float              worldDepth,
        float              maximumHeight,
        TextureId          colorTextureId
    );

    std::vector<float>&       heights();
    const std::vector<float>& heights() const;

    std::size_t& rows();
    const std::size_t& rows() const;

    std::size_t& columns();
    const std::size_t& columns() const;

    float& worldWidth();
    const float& worldWidth() const;

    float& worldDepth();
    const float& worldDepth() const;

    float& maximumHeight();
    const float& maximumHeight() const;

    TextureId& colorTextureId();
    const TextureId& colorTextureId() const;

private:
    std::vector<float> mHeights;
    std::size_t        mRows;
    std::size_t        mColumns;
    float              mWorldWidth;
    float              mWorldDepth;
    float              mMaximumHeight;
    TextureId          mColorTextureId;
};

} // namespace Nothofagus
