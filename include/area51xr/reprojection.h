#pragma once

#include "area51xr/depth_mesh.h"

#include <optional>

namespace area51xr {

struct PixelCoordinate {
    float x{};
    float y{};
};

struct NormalizedCoordinate {
    float x{};
    float y{};
};

std::optional<PixelCoordinate> project_world_point(
    Vec3 world_point,
    Vec3 eye_position,
    const CameraIntrinsics& camera,
    std::uint32_t width,
    std::uint32_t height) noexcept;

std::optional<NormalizedCoordinate> reproject_world_point_uv(
    Vec3 world_point,
    Vec3 eye_position,
    const CameraIntrinsics& camera,
    std::uint32_t width,
    std::uint32_t height) noexcept;

} // namespace area51xr
