#include "area51xr/reprojection.h"

namespace area51xr {

std::optional<PixelCoordinate> project_world_point(
    Vec3 world_point,
    Vec3 eye_position,
    const CameraIntrinsics& camera,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (width == 0 || height == 0 || camera.fx <= 0.0f || camera.fy <= 0.0f) {
        return std::nullopt;
    }

    const Vec3 relative{
        world_point.x - eye_position.x,
        world_point.y - eye_position.y,
        world_point.z - eye_position.z
    };
    const float depth = -relative.z;
    if (depth <= 0.0f) {
        return std::nullopt;
    }

    PixelCoordinate pixel{};
    pixel.x = camera.cx + camera.fx * relative.x / depth;
    pixel.y = camera.cy - camera.fy * relative.y / depth;
    if (pixel.x < 0.0f || pixel.x > static_cast<float>(width - 1) ||
        pixel.y < 0.0f || pixel.y > static_cast<float>(height - 1)) {
        return std::nullopt;
    }
    return pixel;
}

std::optional<NormalizedCoordinate> reproject_world_point_uv(
    Vec3 world_point,
    Vec3 eye_position,
    const CameraIntrinsics& camera,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    const auto pixel = project_world_point(world_point, eye_position, camera, width, height);
    if (!pixel) {
        return std::nullopt;
    }

    return NormalizedCoordinate{
        width > 1 ? pixel->x / static_cast<float>(width - 1) : 0.0f,
        height > 1 ? pixel->y / static_cast<float>(height - 1) : 0.0f
    };
}

} // namespace area51xr
