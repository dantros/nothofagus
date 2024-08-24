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
    glm::vec2 mScale;
    float mAngle;
};

}