#include "area51xr/xr_aim.h"

#include <algorithm>
#include <cmath>

namespace area51xr {

std::optional<NormalizedAim> project_aim_to_plane(
    const Pose& aim_pose,
    const AimPlane& plane) noexcept {
    if (plane.width <= 0.0f || plane.height <= 0.0f) {
        return std::nullopt;
    }

    const Vec3 direction = aim_forward(aim_pose);
    if (std::abs(direction.z) < 1.0e-5f) {
        return std::nullopt;
    }

    const float t = (plane.center.z - aim_pose.position.z) / direction.z;
    if (t <= 0.0f) {
        return std::nullopt;
    }

    const float hit_x = aim_pose.position.x + direction.x * t;
    const float hit_y = aim_pose.position.y + direction.y * t;

    const float left = plane.center.x - plane.width * 0.5f;
    const float top = plane.center.y + plane.height * 0.5f;
    const float normalized_x = (hit_x - left) / plane.width;
    const float normalized_y = (top - hit_y) / plane.height;

    if (normalized_x < 0.0f || normalized_x > 1.0f ||
        normalized_y < 0.0f || normalized_y > 1.0f) {
        return std::nullopt;
    }

    return NormalizedAim{
        std::clamp(normalized_x, 0.0f, 1.0f),
        std::clamp(normalized_y, 0.0f, 1.0f)
    };
}

} // namespace area51xr
