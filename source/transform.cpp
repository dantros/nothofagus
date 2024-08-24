#include "transform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <numbers>

namespace Nothofagus
{

float degreesToRadians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180.0f;
}

glm::mat3 Transform::toMat3() const
{
    glm::mat3 out(1.0);
    out = glm::translate(out, mLocation);
    out = glm::rotate(out, degreesToRadians(mAngle));
    out = glm::scale(out, mScale);
    return out;
}

}