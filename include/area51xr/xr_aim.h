#pragma once

#include "area51xr/xr_runtime.h"

#include <optional>

namespace area51xr {

constexpr float kDefaultScreenDistanceMeters = 1.0f;
constexpr float kDefaultScreenWidthMeters = 1.4f;
constexpr float kDefaultScreenHeightMeters = 1.05f;

struct AimPlane {
    Vec3 center{0.0f, 0.0f, -kDefaultScreenDistanceMeters};
    float width{kDefaultScreenWidthMeters};
    float height{kDefaultScreenHeightMeters};
};

struct NormalizedAim {
    float x{};
    float y{};
};

std::optional<NormalizedAim> project_aim_to_plane(
    const Pose& aim_pose,
    const AimPlane& plane) noexcept;

} // namespace area51xr
