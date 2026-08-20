#include "area51xr/bridge_host.h"

#include <algorithm>

namespace area51xr {

void write_gun_state(MameSharedState& shared, float aim_x, float aim_y, bool trigger) noexcept {
    shared.gun.protocol_version = kMameBridgeProtocolVersion;
    ++shared.gun.sequence;
    shared.gun.aim_x = std::clamp(aim_x, 0.0f, 1.0f);
    shared.gun.aim_y = std::clamp(aim_y, 0.0f, 1.0f);
    shared.gun.trigger = trigger ? 1 : 0;
    ++shared.gun.sequence;
}

FrameStatus read_frame_status(const MameSharedState& shared) noexcept {
    FrameStatus status{};
    if (shared.frame.protocol_version != kMameBridgeProtocolVersion) {
        return status;
    }
    status.frame_number = shared.frame.frame_number;
    status.width = shared.frame.width;
    status.height = shared.frame.height;
    status.payload_bytes = shared.frame.payload_bytes;
    return status;
}

}  // namespace area51xr
