#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <numbers>

namespace Nothofagus
{

/* Angle is represented in degrees */
class Transform
{
public:
    Transform() :
        mLocation(0), mAngle(0), mScale(0)
    {}

    Transform(glm::vec2 location) :
        mLocation(location), mAngle(0), mScale(0)
    {}

    Transform(glm::vec2 location, float angle) :
        mLocation(location), mAngle(angle), mScale(0)
    {}

    Transform(glm::vec2 location, float angle, float uniformScale) :
        mLocation(location), mAngle(angle), mScale(uniformScale, uniformScale)
    {}

    Transform(glm::vec2 location, float angle, glm::vec2 scale) :
        mLocation(location), mAngle(angle), mScale(scale)
    {}

    Transform(const glm::mat3& mat)
    {
        throw;
        /*mTx = mat[];
        mTy = ;
        mRx = ;
        mSx = ;
        mSy = ;*/
    }

    glm::mat3 toMat3() const
    {
        glm::mat3 out(1.0);
        out = glm::translate(out, mLocation);
        out = glm::rotate(out, degreesToRadians(mAngle));
        out = glm::scale(out, mScale);
        return out;
    }

    glm::vec2& location(){ return mLocation; }
    const glm::vec2& location() const{ return mLocation; }

    float& angle(){ return mAngle; }
    const float& angle() const{ return mAngle; }

    glm::vec2& scale(){ return mScale; }
    const glm::vec2& scale() const{ return mScale; }

private:
    static float degreesToRadians(float degrees)
    {
        return degrees * std::numbers::pi_v<float> / 180.0f;
    }

    glm::vec2 mLocation;
    float mAngle;
    glm::vec2 mScale;
};

}