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

void SimulatedXrRuntime::shutdown() {
    initialized_ = false;
}

void SimulatedXrRuntime::set_state(const XrInputState& state) {
    state_ = state;
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
