#include "core/projection.hpp"

#include "area51xr/depth_mesh.h"
#include "area51xr/mame_ipc.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>

namespace {

bool near(double a, double b) {
    return std::abs(a - b) < 0.0001;
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

int test_projection() {
    const auto center = a51xr::normalized_to_gun({0.5, 0.5}, {640, 480});
    if (!center || !near(center->x, 319.5) || !near(center->y, 239.5)) {
        return fail("center projection failed");
    }

    const auto upper_left = a51xr::normalized_to_gun({0.0, 1.0}, {640, 480});
    if (!upper_left || !near(upper_left->x, 0.0) || !near(upper_left->y, 0.0)) {
        return fail("upper-left projection failed");
    }

    if (a51xr::normalized_to_gun({-0.01, 0.5}, {640, 480})) {
        return fail("out-of-range input was accepted");
    }

    if (a51xr::normalized_to_gun({0.5, 0.5}, {0, 480})) {
        return fail("invalid bounds were accepted");
    }

    return 0;
}

int test_mame_ipc() {
    // MameSharedState contains the full shared framebuffer and is intentionally
    // larger than the default Windows thread stack. Keep the test fixture on
    // the heap just like the real memory-mapped instance.
    auto shared = std::make_unique<area51xr::MameSharedState>();

    area51xr::MameFrameHeader header{};
    header.frame_number = 42;
    header.width = 2;
    header.height = 1;
    header.stride_bytes = 8;
    header.pixel_format = 1;

    const std::array<std::uint8_t, 8> pixels{1, 2, 3, 4, 5, 6, 7, 8};
    header.payload_bytes = pixels.size();

    if (!area51xr::publish_frame(*shared, header, pixels)) {
        return fail("MAME frame publish failed");
    }
    if (shared->frame.frame_number != 42 || (shared->frame.sequence & 1u) != 0) {
        return fail("MAME frame publish metadata was invalid");
    }

    std::array<std::uint8_t, 8> copied{};
    area51xr::MameFrameHeader copied_header{};
    if (!area51xr::copy_latest_frame(*shared, copied_header, copied)) {
        return fail("MAME frame copy failed");
    }
    if (copied_header.frame_number != 42 || copied_header.width != 2 ||
        copied_header.height != 1 || copied != pixels) {
        return fail("MAME copied frame did not match published frame");
    }

    auto bad_header = header;
    bad_header.payload_bytes = pixels.size() + 1;
    if (area51xr::publish_frame(*shared, bad_header, pixels)) {
        return fail("MAME bridge accepted an oversized payload declaration");
    }

    std::array<std::uint8_t, 4> too_small{};
    if (area51xr::copy_latest_frame(*shared, copied_header, too_small)) {
        return fail("MAME bridge copied into an undersized destination");
    }

#ifdef _WIN32
    area51xr::MameIpc owner;
    if (!owner.create() || !owner.valid()) {
        return fail("MAME IPC owner creation failed");
    }

    owner.state()->gun.sequence = 8;
    owner.state()->gun.aim_x = 0.25f;
    owner.state()->gun.aim_y = 0.75f;

    area51xr::MameIpc client;
    if (!client.open() || !client.valid()) {
        return fail("MAME IPC client open failed");
    }
    if (client.state()->gun.sequence != 8 ||
        client.state()->gun.aim_x != 0.25f ||
        client.state()->gun.aim_y != 0.75f) {
        return fail("MAME IPC shared gun state mismatch");
    }
#endif

    return 0;
}

int test_depth_mesh() {
    const auto camera = area51xr::intrinsics_from_vertical_fov(2, 2, 1.57079632679f);
    if (!near(camera.cx, 0.5f) || !near(camera.cy, 0.5f) ||
        camera.fx <= 0.0f || camera.fy <= 0.0f) {
        return fail("depth mesh camera intrinsics failed");
    }

    const std::array<float, 4> plane{2.0f, 2.0f, 2.0f, 2.0f};
    const auto mesh = area51xr::build_depth_mesh(plane, 2, 2, camera);
    if (mesh.vertices.size() != 4 || mesh.indices.size() != 6 ||
        !near(mesh.vertices[0].position.z, -2.0f) ||
        !near(mesh.vertices[3].position.z, -2.0f) ||
        !near(mesh.vertices[0].u, 0.0f) ||
        !near(mesh.vertices[3].u, 1.0f) ||
        !near(mesh.vertices[3].v, 1.0f)) {
        return fail("flat depth mesh generation failed");
    }

    area51xr::DepthMeshOptions strict{};
    strict.max_triangle_depth_delta_m = 0.25f;
    const std::array<float, 4> edge{1.0f, 3.0f, 1.0f, 3.0f};
    if (!area51xr::build_depth_mesh(edge, 2, 2, camera, strict).indices.empty()) {
        return fail("depth discontinuity rejection failed");
    }

    const std::array<float, 4> invalid{0.0f, 2.0f, 2.0f, 2.0f};
    if (area51xr::build_depth_mesh(invalid, 2, 2, camera).indices.size() != 3) {
        return fail("depth mesh hole handling failed");
    }

    area51xr::DepthMeshOptions sampled{};
    sampled.sample_step = 2;
    const std::array<float, 9> three_by_three{
        2.0f, 2.0f, 2.0f,
        2.0f, 2.0f, 2.0f,
        2.0f, 2.0f, 2.0f
    };
    const auto coarse = area51xr::build_depth_mesh(
        three_by_three,
        3,
        3,
        area51xr::intrinsics_from_vertical_fov(3, 3, 1.0f),
        sampled);
    if (coarse.vertices.size() != 4 || coarse.indices.size() != 6) {
        return fail("sampled depth mesh generation failed");
    }

    return 0;
}

} // namespace

int main() {
    if (const int result = test_projection(); result != 0) return result;
    if (const int result = test_mame_ipc(); result != 0) return result;
    if (const int result = test_depth_mesh(); result != 0) return result;
    return 0;
}
