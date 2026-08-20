#include "area51xr/xr_host.h"

namespace area51xr {

XrHost::XrHost(XrRuntime& runtime, MameSharedState& shared, AimPlane plane)
    : runtime_(runtime), shared_(shared), plane_(plane) {}

bool XrHost::initialize() {
    initialized_ = runtime_.initialize();
    return initialized_;
}

HostTickResult XrHost::tick() {
    HostTickResult result{};
    result.frame = read_frame_status(shared_);

    if (!initialized_) {
        return result;
    }

    XrInputState input{};
    result.runtime_ok = runtime_.poll(input);
    if (!result.runtime_ok) {
        return result;
    }

    result.session_running = input.session_running;
    result.trigger_down = input.trigger_down;

    if (input.session_running && input.pose_valid) {
        if (const auto projected = project_aim_to_plane(input.aim, plane_)) {
            last_aim_ = *projected;
            result.aim_valid = true;
            result.aim = *projected;
        }
    }

    if (!result.aim_valid) {
        result.aim = last_aim_;
    }

    write_gun_state(
        shared_,
        result.aim.x,
        result.aim.y,
        result.session_running && input.pose_valid && result.trigger_down);

    return result;
}

void XrHost::shutdown() {
    if (initialized_) {
        runtime_.shutdown();
    }
    initialized_ = false;
}

} // namespace area51xr
