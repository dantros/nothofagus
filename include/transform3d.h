#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nothofagus
{

/**
 * @class Transform3D
 * @brief 3D transform for world-space objects (billboard bellotas).
 *
 * Location is a world-space position (vec3).
 * Scale x/y map to billboard width/height in world units when multiplied by texture size.
 * Rotation is a quaternion applied to the camera-aligned billboard axes, allowing
 * billboard-plane rotation (identity = fully camera-aligned, no local rotation).
 */
class Transform3D
{
public:
    Transform3D() :
        mLocation(0.0f), mScale(1.0f), mRotation(glm::identity<glm::quat>())
    {}

    Transform3D(glm::vec3 location) :
        mLocation(location), mScale(1.0f), mRotation(glm::identity<glm::quat>())
    {}

    Transform3D(glm::vec3 location, glm::vec3 scale) :
        mLocation(location), mScale(scale), mRotation(glm::identity<glm::quat>())
    {}

    Transform3D(glm::vec3 location, glm::vec3 scale, glm::quat rotation) :
        mLocation(location), mScale(scale), mRotation(rotation)
    {}

    glm::vec3&       location()       { return mLocation; }
    const glm::vec3& location() const { return mLocation; }

    glm::vec3&       scale()       { return mScale; }
    const glm::vec3& scale() const { return mScale; }

    glm::quat&       rotation()       { return mRotation; }
    const glm::quat& rotation() const { return mRotation; }

private:
    glm::vec3 mLocation;
    glm::vec3 mScale;
    glm::quat mRotation;
};

} // namespace Nothofagus
