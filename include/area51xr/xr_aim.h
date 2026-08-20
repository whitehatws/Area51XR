#pragma once

#include "area51xr/xr_runtime.h"

#include <optional>

namespace area51xr {

struct AimPlane {
    Vec3 center{0.0f, 0.0f, -1.0f};
    float width{1.6f};
    float height{0.9f};
};

struct NormalizedAim {
    float x{};
    float y{};
};

std::optional<NormalizedAim> project_aim_to_plane(
    const Pose& aim_pose,
    const AimPlane& plane) noexcept;

} // namespace area51xr
