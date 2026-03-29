#pragma once

#include "bellota.h"  // for TextureId
#include <glm/glm.hpp>
#include <cstddef>

namespace Nothofagus
{

struct WorldBellotaId
{
    std::size_t id;

    bool operator==(const WorldBellotaId& rhs) const { return id == rhs.id; }
};

/**
 * @class WorldBellota
 * @brief A billboard sprite positioned in 3D world space.
 *
 * A WorldBellota is rendered as a camera-facing quad (spherical billboard).
 * Its position is a 3D world-space coordinate; size is in world units.
 * It participates in GL depth testing against the terrain and other world objects.
 *
 * Unlike screen-space Bellotas, WorldBellotas have no depthOffset — depth ordering
 * is handled entirely by the hardware depth buffer.
 */
class WorldBellota
{
public:
    WorldBellota(glm::vec3 position, glm::vec2 size, TextureId textureId);

    glm::vec3&       position();
    const glm::vec3& position() const;

    glm::vec2&       size();
    const glm::vec2& size() const;

    TextureId        textureId() const;

    float&       opacity();
    const float& opacity() const;

    bool&       visible();
    const bool& visible() const;

    std::size_t&       currentLayer();
    const std::size_t& currentLayer() const;

private:
    glm::vec3   mPosition;
    glm::vec2   mSize;          ///< Billboard width and height in world units
    TextureId   mTextureId;
    float       mOpacity;
    bool        mVisible;
    std::size_t mCurrentLayer;
};

} // namespace Nothofagus
