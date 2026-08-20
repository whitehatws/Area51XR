#include "area51xr/bridge_host.h"

#include <algorithm>
#include <atomic>

namespace area51xr {

void write_gun_state(MameSharedState& shared, float aim_x, float aim_y, bool trigger) noexcept {
    std::atomic_ref<std::uint64_t> sequence(shared.gun.sequence);
    sequence.fetch_add(1, std::memory_order_acq_rel);

    shared.gun.protocol_version = kMameBridgeProtocolVersion;
    shared.gun.aim_x = std::clamp(aim_x, 0.0f, 1.0f);
    shared.gun.aim_y = std::clamp(aim_y, 0.0f, 1.0f);
    shared.gun.trigger = trigger ? 1 : 0;

    sequence.fetch_add(1, std::memory_order_release);
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
