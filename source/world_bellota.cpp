#include "../include/world_bellota.h"

namespace Nothofagus
{

WorldBellota::WorldBellota(glm::vec3 position, glm::vec2 size, TextureId textureId)
    : mPosition{position}
    , mSize{size}
    , mTextureId{textureId}
    , mOpacity{1.0f}
    , mVisible{true}
    , mCurrentLayer{0}
{
}

glm::vec3&       WorldBellota::position()       { return mPosition; }
const glm::vec3& WorldBellota::position() const { return mPosition; }

glm::vec2&       WorldBellota::size()       { return mSize; }
const glm::vec2& WorldBellota::size() const { return mSize; }

TextureId WorldBellota::textureId() const { return mTextureId; }

float&       WorldBellota::opacity()       { return mOpacity; }
const float& WorldBellota::opacity() const { return mOpacity; }

bool&       WorldBellota::visible()       { return mVisible; }
const bool& WorldBellota::visible() const { return mVisible; }

std::size_t&       WorldBellota::currentLayer()       { return mCurrentLayer; }
const std::size_t& WorldBellota::currentLayer() const { return mCurrentLayer; }

} // namespace Nothofagus
