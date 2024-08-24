#pragma once

#include <cstddef>
#include <glm/glm.hpp>

namespace Nothofagus
{

/* Angle is represented in degrees */
class Transform
{
public:
    Transform() :
        mLocation(0), mScale(1.0), mAngle(0)
    {}

    Transform(glm::vec2 location) :
        mLocation(location), mScale(1.0), mAngle(0)
    {}

    Transform(glm::vec2 location, float uniformScale) :
        mLocation(location), mScale(uniformScale, uniformScale), mAngle(0.0)
    {}

    Transform(glm::vec2 location, float uniformScale, float angle) :
        mLocation(location), mScale(uniformScale, uniformScale), mAngle(angle)
    {}

    Transform(glm::vec2 location, glm::vec2 scale, float angle) :
        mLocation(location), mScale(scale), mAngle(angle)
    {}

    glm::mat3 toMat3() const;

    glm::vec2& location(){ return mLocation; }
    const glm::vec2& location() const{ return mLocation; }

    float& angle(){ return mAngle; }
    const float& angle() const{ return mAngle; }

    glm::vec2& scale(){ return mScale; }
    const glm::vec2& scale() const{ return mScale; }

private:
    glm::vec2 mLocation;
    glm::vec2 mScale;
    float mAngle;
};

}