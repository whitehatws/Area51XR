#include "area51xr/depth_mesh.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {
bool near(float a, float b) { return std::abs(a - b) < 1.0e-4f; }
}

int main() {
    const auto camera = area51xr::intrinsics_from_vertical_fov(2, 2, 1.57079632679f);
    assert(near(camera.cx, 0.5f));
    assert(near(camera.cy, 0.5f));
    assert(camera.fx > 0.0f);
    assert(camera.fy > 0.0f);

    const std::array<float, 4> plane{2.0f, 2.0f, 2.0f, 2.0f};
    const auto mesh = area51xr::build_depth_mesh(plane, 2, 2, camera);
    assert(mesh.vertices.size() == 4);
    assert(mesh.indices.size() == 6);
    assert(near(mesh.vertices[0].position.z, -2.0f));
    assert(near(mesh.vertices[3].position.z, -2.0f));
    assert(near(mesh.vertices[0].u, 0.0f));
    assert(near(mesh.vertices[3].u, 1.0f));
    assert(near(mesh.vertices[3].v, 1.0f));

    area51xr::DepthMeshOptions strict{};
    strict.max_triangle_depth_delta_m = 0.25f;
    const std::array<float, 4> edge{1.0f, 3.0f, 1.0f, 3.0f};
    const auto split = area51xr::build_depth_mesh(edge, 2, 2, camera, strict);
    assert(split.indices.empty());

    // With the mesh winding 0,2,1 / 1,2,3, corner 0 belongs only to the
    // first triangle. Invalidating it should preserve the second triangle.
    const std::array<float, 4> invalid{0.0f, 2.0f, 2.0f, 2.0f};
    const auto holes = area51xr::build_depth_mesh(invalid, 2, 2, camera);
    assert(holes.indices.size() == 3);

    area51xr::DepthMeshOptions sampled{};
    sampled.sample_step = 2;
    const std::array<float, 9> three_by_three{
        2.0f, 2.0f, 2.0f,
        2.0f, 2.0f, 2.0f,
        2.0f, 2.0f, 2.0f
    };
    const auto coarse = area51xr::build_depth_mesh(three_by_three, 3, 3,
        area51xr::intrinsics_from_vertical_fov(3, 3, 1.0f), sampled);
    assert(coarse.vertices.size() == 4);
    assert(coarse.indices.size() == 6);

    return 0;
}
