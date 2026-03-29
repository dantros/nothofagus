#pragma once

#include <glm/glm.hpp>

namespace Nothofagus
{

/**
 * @class Camera
 * @brief Perspective camera for 3D rendering.
 *
 * The Canvas owns one Camera instance accessible via canvas.camera().
 * Aspect ratio is not stored here — it is computed from the game viewport
 * each frame and passed to toProjectionMatrix().
 *
 * Default orientation: position (0, 5, 10), looking at the origin,
 * 60-degree field of view.
 */
class Camera
{
public:
    Camera();

    Camera(
        glm::vec3 position,
        glm::vec3 target,
        glm::vec3 upVector,
        float fieldOfViewDegrees,
        float nearPlane,
        float farPlane
    );

    glm::vec3&       position();
    const glm::vec3& position() const;

    glm::vec3&       target();
    const glm::vec3& target() const;

    glm::vec3&       upVector();
    const glm::vec3& upVector() const;

    float&       fieldOfViewDegrees();
    const float& fieldOfViewDegrees() const;

    float&       nearPlane();
    const float& nearPlane() const;

    float&       farPlane();
    const float& farPlane() const;

    /** @brief Returns the view matrix (glm::lookAt). */
    glm::mat4 toViewMatrix() const;

    /** @brief Returns the perspective projection matrix for the given aspect ratio. */
    glm::mat4 toProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 mPosition;
    glm::vec3 mTarget;
    glm::vec3 mUpVector;
    float     mFieldOfViewDegrees;
    float     mNearPlane;
    float     mFarPlane;
};

} // namespace Nothofagus
