#include "../include/bellota.h"
#include "check.h"

namespace Nothofagus
{

const Transform& Bellota::transform() const
{
    debugCheck(std::holds_alternative<Transform>(mTransform),
        "Bellota::transform() called on a world-space (Transform3D) bellota. Use transform3d() instead.");
    return std::get<Transform>(mTransform);
}

Transform& Bellota::transform()
{
    debugCheck(std::holds_alternative<Transform>(mTransform),
        "Bellota::transform() called on a world-space (Transform3D) bellota. Use transform3d() instead.");
    return std::get<Transform>(mTransform);
}

const Transform3D& Bellota::transform3d() const
{
    debugCheck(std::holds_alternative<Transform3D>(mTransform),
        "Bellota::transform3d() called on a screen-space (Transform) bellota. Use transform() instead.");
    return std::get<Transform3D>(mTransform);
}

Transform3D& Bellota::transform3d()
{
    debugCheck(std::holds_alternative<Transform3D>(mTransform),
        "Bellota::transform3d() called on a screen-space (Transform) bellota. Use transform() instead.");
    return std::get<Transform3D>(mTransform);
}

} // namespace Nothofagus
