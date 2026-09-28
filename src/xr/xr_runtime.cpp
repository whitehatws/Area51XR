#include "area51xr/xr_runtime.h"

namespace area51xr {

SimulatedXrRuntime::SimulatedXrRuntime(XrInputState initial) : state_(initial) {}

bool SimulatedXrRuntime::initialize() {
    initialized_ = true;
    return true;
}

bool SimulatedXrRuntime::poll(XrInputState& state) {
    if (!initialized_) {
        return false;
    }
    state = state_;
    ++state_.sample_number;
    return true;
}

bool SimulatedXrRuntime::present(const VideoFrameView& frame) {
    if (!initialized_) {
        return false;
    }
    last_presented_frame_ = frame.frame_number;
    last_presented_pixels_.assign(frame.pixels.begin(), frame.pixels.end());
    return true;
}

bool SimulatedXrRuntime::present_mesh(const SpatialMeshView& mesh) {
    if (!initialized_) {
        return false;
    }
    last_presented_mesh_frame_ = mesh.frame_number;
    last_presented_mesh_vertices_ = mesh.vertices.size();
    last_presented_mesh_triangles_ = mesh.indices.size() / 3;
    return true;
}

PassthroughState SimulatedXrRuntime::passthrough_state() const noexcept {
    return passthrough_state_;
}

const char* SimulatedXrRuntime::passthrough_extension() const noexcept {
    return passthrough_state_ == PassthroughState::unavailable ? "" : "SIMULATED_passthrough";
}

bool SimulatedXrRuntime::set_passthrough_enabled(bool enabled) {
    if (!initialized_ || passthrough_state_ == PassthroughState::unavailable)
        return false;
    if (enabled && passthrough_enable_failure_) {
        passthrough_state_ = PassthroughState::unavailable;
        return false;
    }
    passthrough_state_ = enabled ? PassthroughState::on : PassthroughState::off;
    return true;
}

void SimulatedXrRuntime::shutdown() {
    initialized_ = false;
}

void SimulatedXrRuntime::set_state(const XrInputState& state) {
    state_ = state;
}

void SimulatedXrRuntime::set_passthrough_supported(bool supported) {
    passthrough_state_ = supported ? PassthroughState::off : PassthroughState::unavailable;
}

void SimulatedXrRuntime::set_passthrough_enable_failure(bool fail) {
    passthrough_enable_failure_ = fail;
}

std::uint64_t SimulatedXrRuntime::last_presented_frame() const noexcept {
    return last_presented_frame_;
}

std::uint64_t SimulatedXrRuntime::last_presented_mesh_frame() const noexcept {
    return last_presented_mesh_frame_;
}

std::size_t SimulatedXrRuntime::last_presented_mesh_vertices() const noexcept {
    return last_presented_mesh_vertices_;
}

std::size_t SimulatedXrRuntime::last_presented_mesh_triangles() const noexcept {
    return last_presented_mesh_triangles_;
}

std::span<const std::uint8_t> SimulatedXrRuntime::last_presented_pixels() const noexcept {
    return last_presented_pixels_;
}

Vec3 rotate_vector(const Quat& q, Vec3 v) noexcept {
    const Vec3 qv{q.x, q.y, q.z};
    const Vec3 uv{
        qv.y * v.z - qv.z * v.y,
        qv.z * v.x - qv.x * v.z,
        qv.x * v.y - qv.y * v.x
    };
    const Vec3 uuv{
        qv.y * uv.z - qv.z * uv.y,
        qv.z * uv.x - qv.x * uv.z,
        qv.x * uv.y - qv.y * uv.x
    };

    const float two_w = 2.0f * q.w;
    return {
        v.x + two_w * uv.x + 2.0f * uuv.x,
        v.y + two_w * uv.y + 2.0f * uuv.y,
        v.z + two_w * uv.z + 2.0f * uuv.z
    };
}

Vec3 aim_forward(const Pose& pose) noexcept {
    return rotate_vector(pose.orientation, {0.0f, 0.0f, -1.0f});
}

} // namespace area51xr
