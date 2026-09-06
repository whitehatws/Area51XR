#include "area51xr/xr_runtime.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 1.0e-4f;
}

} // namespace

int main() {
    area51xr::Pose identity{};
    const auto forward = area51xr::aim_forward(identity);
    assert(near(forward.x, 0.0f));
    assert(near(forward.y, 0.0f));
    assert(near(forward.z, -1.0f));

    const float s = std::sqrt(0.5f);
    area51xr::Pose right{};
    right.orientation = {0.0f, -s, 0.0f, s};
    const auto right_forward = area51xr::aim_forward(right);
    assert(near(right_forward.x, 1.0f));
    assert(near(right_forward.y, 0.0f));
    assert(near(right_forward.z, 0.0f));

    area51xr::XrInputState initial{};
    initial.session_running = true;
    initial.pose_valid = true;
    initial.trigger_down = true;
    initial.coin_down = true;
    initial.start_down = true;
    initial.menu_down = true;
    initial.active_hand = area51xr::ControllerHand::left;
    initial.left_pose_valid = true;
    initial.left_trigger_down = true;
    initial.left_coin_down = true;
    initial.left_start_down = true;
    initial.left_menu_down = true;
    initial.sample_number = 7;
    area51xr::SimulatedXrRuntime runtime(initial);
    assert(runtime.passthrough_state() == area51xr::PassthroughState::unavailable);
    assert(runtime.initialize());

    area51xr::XrInputState sampled{};
    assert(runtime.poll(sampled));
    assert(sampled.sample_number == 7);
    assert(sampled.session_running);
    assert(sampled.pose_valid);
    assert(sampled.trigger_down);
    assert(sampled.coin_down);
    assert(sampled.start_down);
    assert(sampled.menu_down);
    assert(sampled.active_hand == area51xr::ControllerHand::left);
    assert(sampled.left_pose_valid);
    assert(sampled.left_trigger_down);
    assert(sampled.left_coin_down);
    assert(sampled.left_start_down);
    assert(sampled.left_menu_down);

    runtime.set_passthrough_supported(true);
    assert(runtime.passthrough_state() == area51xr::PassthroughState::off);
    assert(runtime.set_passthrough_enabled(true));
    assert(runtime.passthrough_state() == area51xr::PassthroughState::on);
    assert(runtime.set_passthrough_enabled(false));
    assert(runtime.passthrough_state() == area51xr::PassthroughState::off);

    runtime.set_passthrough_enable_failure(true);
    assert(!runtime.set_passthrough_enabled(true));
    assert(runtime.passthrough_state() == area51xr::PassthroughState::unavailable);

    initial.session_running = false;
    runtime.set_state(initial);
    assert(runtime.poll(sampled));
    assert(!sampled.session_running);

    const std::array<area51xr::SpatialMeshVertexView, 3> vertices{{
        {{0.0f, 0.0f, -1.0f}, 0.0f, 0.0f},
        {{1.0f, 0.0f, -1.0f}, 1.0f, 0.0f},
        {{0.0f, 1.0f, -1.0f}, 0.0f, 1.0f}
    }};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    area51xr::SpatialMeshView mesh{};
    mesh.frame_number = 99;
    mesh.vertices = vertices;
    mesh.indices = indices;
    assert(runtime.present_mesh(mesh));
    assert(runtime.last_presented_mesh_frame() == 99);
    assert(runtime.last_presented_mesh_vertices() == 3);
    assert(runtime.last_presented_mesh_triangles() == 1);

    runtime.shutdown();
    assert(!runtime.poll(sampled));
    assert(!runtime.present_mesh(mesh));
    return 0;
}
