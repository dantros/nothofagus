#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Nothofagus
{

Camera::Camera()
    : mPosition{0.0f, 5.0f, 10.0f}
    , mTarget{0.0f, 0.0f, 0.0f}
    , mUpVector{0.0f, 1.0f, 0.0f}
    , mFieldOfViewDegrees{60.0f}
    , mNearPlane{0.1f}
    , mFarPlane{1000.0f}
{
}

Camera::Camera(
    glm::vec3 position,
    glm::vec3 target,
    glm::vec3 upVector,
    float     fieldOfViewDegrees,
    float     nearPlane,
    float     farPlane)
    : mPosition{position}
    , mTarget{target}
    , mUpVector{upVector}
    , mFieldOfViewDegrees{fieldOfViewDegrees}
    , mNearPlane{nearPlane}
    , mFarPlane{farPlane}
{
}

glm::vec3&       Camera::position()             { return mPosition; }
const glm::vec3& Camera::position() const       { return mPosition; }

glm::vec3&       Camera::target()               { return mTarget; }
const glm::vec3& Camera::target() const         { return mTarget; }

glm::vec3&       Camera::upVector()             { return mUpVector; }
const glm::vec3& Camera::upVector() const       { return mUpVector; }

float&       Camera::fieldOfViewDegrees()       { return mFieldOfViewDegrees; }
const float& Camera::fieldOfViewDegrees() const { return mFieldOfViewDegrees; }

float&       Camera::nearPlane()                { return mNearPlane; }
const float& Camera::nearPlane() const          { return mNearPlane; }

float&       Camera::farPlane()                 { return mFarPlane; }
const float& Camera::farPlane() const           { return mFarPlane; }

glm::mat4 Camera::toViewMatrix() const
{
    return glm::lookAt(mPosition, mTarget, mUpVector);
}

glm::mat4 Camera::toProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(mFieldOfViewDegrees), aspectRatio, mNearPlane, mFarPlane);
}

} // namespace Nothofagus
