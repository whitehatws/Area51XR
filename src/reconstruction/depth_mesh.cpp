#include "area51xr/depth_mesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace area51xr {
namespace {

bool valid_depth(float depth, const DepthMeshOptions& options) noexcept {
    return std::isfinite(depth) &&
           depth >= options.min_depth_m &&
           depth <= options.max_depth_m;
}

DepthMeshVertex make_vertex(
    std::uint32_t x,
    std::uint32_t y,
    float depth,
    std::uint32_t width,
    std::uint32_t height,
    const CameraIntrinsics& camera) noexcept {
    const float px = (static_cast<float>(x) - camera.cx) * depth / camera.fx;
    const float py = -(static_cast<float>(y) - camera.cy) * depth / camera.fy;
    const float u = width > 1 ? static_cast<float>(x) / static_cast<float>(width - 1) : 0.0f;
    const float v = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
    return {{px, py, -depth}, u, v};
}

bool triangle_is_continuous(
    float a,
    float b,
    float c,
    const DepthMeshOptions& options) noexcept {
    if (!valid_depth(a, options) || !valid_depth(b, options) || !valid_depth(c, options)) {
        return false;
    }
    const float minimum = std::min({a, b, c});
    const float maximum = std::max({a, b, c});
    return maximum - minimum <= options.max_triangle_depth_delta_m;
}

} // namespace

CameraIntrinsics intrinsics_from_vertical_fov(
    std::uint32_t width,
    std::uint32_t height,
    float vertical_fov_radians) noexcept {
    CameraIntrinsics result{};
    if (width == 0 || height == 0 || vertical_fov_radians <= 0.0f ||
        vertical_fov_radians >= 3.14159265f) {
        return result;
    }

    const float half_height = static_cast<float>(height) * 0.5f;
    result.fy = half_height / std::tan(vertical_fov_radians * 0.5f);
    result.fx = result.fy;
    result.cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    result.cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    return result;
}

DepthMesh build_depth_mesh(
    std::span<const float> depth_m,
    std::uint32_t width,
    std::uint32_t height,
    const CameraIntrinsics& camera,
    const DepthMeshOptions& options) {
    DepthMesh mesh{};
    if (width == 0 || height == 0 || depth_m.size() != static_cast<std::size_t>(width) * height ||
        camera.fx <= 0.0f || camera.fy <= 0.0f || options.sample_step == 0) {
        return mesh;
    }

    const std::uint32_t step = options.sample_step;
    const std::uint32_t columns = (width - 1) / step + 1;
    const std::uint32_t rows = (height - 1) / step + 1;
    mesh.vertices.reserve(static_cast<std::size_t>(columns) * rows);

    std::vector<float> sampled_depth;
    sampled_depth.reserve(static_cast<std::size_t>(columns) * rows);

    for (std::uint32_t row = 0; row < rows; ++row) {
        const std::uint32_t y = std::min(row * step, height - 1);
        for (std::uint32_t column = 0; column < columns; ++column) {
            const std::uint32_t x = std::min(column * step, width - 1);
            const float depth = depth_m[static_cast<std::size_t>(y) * width + x];
            sampled_depth.push_back(depth);
            mesh.vertices.push_back(make_vertex(x, y, depth, width, height, camera));
        }
    }

    if (columns < 2 || rows < 2) {
        return mesh;
    }

    for (std::uint32_t row = 0; row + 1 < rows; ++row) {
        for (std::uint32_t column = 0; column + 1 < columns; ++column) {
            const std::uint32_t i00 = row * columns + column;
            const std::uint32_t i10 = i00 + 1;
            const std::uint32_t i01 = i00 + columns;
            const std::uint32_t i11 = i01 + 1;

            const float d00 = sampled_depth[i00];
            const float d10 = sampled_depth[i10];
            const float d01 = sampled_depth[i01];
            const float d11 = sampled_depth[i11];

            if (triangle_is_continuous(d00, d01, d10, options)) {
                mesh.indices.insert(mesh.indices.end(), {i00, i01, i10});
            }
            if (triangle_is_continuous(d10, d01, d11, options)) {
                mesh.indices.insert(mesh.indices.end(), {i10, i01, i11});
            }
        }
    }

    return mesh;
}

} // namespace area51xr
