#include "area51xr/reprojection.h"

#include <cassert>
#include <cmath>

namespace {
bool near(float a, float b) { return std::abs(a - b) < 1.0e-4f; }
}

int main() {
    const auto camera = area51xr::intrinsics_from_vertical_fov(101, 101, 1.57079632679f);
    const area51xr::Vec3 point{0.0f, 0.0f, -2.0f};

    const auto center = area51xr::project_world_point(point, {0.0f, 0.0f, 0.0f}, camera, 101, 101);
    assert(center);
    assert(near(center->x, 50.0f));
    assert(near(center->y, 50.0f));

    const auto moved_right = area51xr::project_world_point(point, {0.1f, 0.0f, 0.0f}, camera, 101, 101);
    assert(moved_right);
    assert(moved_right->x < center->x);

    const auto moved_up = area51xr::project_world_point(point, {0.0f, 0.1f, 0.0f}, camera, 101, 101);
    assert(moved_up);
    assert(moved_up->y > center->y);

    const auto normalized = area51xr::reproject_world_point_uv(point, {0.0f, 0.0f, 0.0f}, camera, 101, 101);
    assert(normalized);
    assert(near(normalized->x, 0.5f));
    assert(near(normalized->y, 0.5f));

    assert(!area51xr::project_world_point({0.0f, 0.0f, 1.0f}, {}, camera, 101, 101));
    return 0;
}
