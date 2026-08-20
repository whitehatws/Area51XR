#include "area51xr/xr_aim.h"

#include <cassert>
#include <cmath>

namespace {
bool near(float a, float b) { return std::abs(a - b) < 1.0e-4f; }
}

int main() {
    area51xr::AimPlane plane{};
    area51xr::Pose center{};
    center.position = {0.0f, 0.0f, 0.0f};
    const auto center_hit = area51xr::project_aim_to_plane(center, plane);
    assert(center_hit);
    assert(near(center_hit->x, 0.5f));
    assert(near(center_hit->y, 0.5f));

    area51xr::Pose left{};
    left.position = {-0.4f, 0.0f, 0.0f};
    const auto left_hit = area51xr::project_aim_to_plane(left, plane);
    assert(left_hit);
    assert(near(left_hit->x, 0.25f));

    area51xr::Pose outside{};
    outside.position = {1.0f, 0.0f, 0.0f};
    assert(!area51xr::project_aim_to_plane(outside, plane));

    area51xr::Pose backwards{};
    backwards.orientation = {0.0f, 1.0f, 0.0f, 0.0f};
    assert(!area51xr::project_aim_to_plane(backwards, plane));
    return 0;
}
