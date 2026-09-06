#pragma once

#include "area51xr/xr_runtime.h"

#include <cstdint>
#include <span>
#include <vector>

namespace area51xr {

struct CameraIntrinsics {
    float fx{};
    float fy{};
    float cx{};
    float cy{};
};

struct DepthMeshVertex {
    Vec3 position{};
    float u{};
    float v{};
};

struct DepthMesh {
    std::vector<DepthMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct DepthMeshOptions {
    std::uint32_t sample_step{1};
    float min_depth_m{0.1f};
    float max_depth_m{20.0f};
    float max_triangle_depth_delta_m{0.35f};
};

CameraIntrinsics intrinsics_from_vertical_fov(
    std::uint32_t width,
    std::uint32_t height,
    float vertical_fov_radians) noexcept;

DepthMesh build_depth_mesh(
    std::span<const float> depth_m,
    std::uint32_t width,
    std::uint32_t height,
    const CameraIntrinsics& intrinsics,
    const DepthMeshOptions& options = {});

} // namespace area51xr
