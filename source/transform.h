#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>

namespace Nothofagus
{

class Transform
{
public:
    Transform() = default;

    Transform(glm::vec2 location):
        mLocation(location), mAngle(0), mScale(0)
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
        glm::translate(out, mLocation);
        glm::rotate(out, mAngle);
        glm::scale(out, mScale);
        return out;
    }

    glm::vec2& location(){ return mLocation; }
    const glm::vec2& location() const{ return mLocation; }

    float& angle(){ return mAngle; }
    const float& angle() const{ return mAngle; }

    glm::vec2& scale(){ return mScale; }
    const glm::vec2& scale() const{ return mScale; }

private:
    glm::vec2 mLocation;
    float mAngle;
    glm::vec2 mScale;
};

}