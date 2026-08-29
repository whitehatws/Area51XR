#include "area51xr/bridge_host.h"

#include <algorithm>
#include <atomic>

namespace area51xr {

void write_gun_state(
    MameSharedState& shared,
    float aim_x,
    float aim_y,
    bool trigger,
    bool offscreen,
    bool coin,
    bool start) noexcept {
    std::atomic_ref<std::uint64_t> sequence(shared.gun.sequence);
    sequence.fetch_add(1, std::memory_order_acq_rel);

    shared.gun.protocol_version = kMameBridgeProtocolVersion;
    shared.gun.aim_x = std::clamp(aim_x, 0.0f, 1.0f);
    shared.gun.aim_y = std::clamp(aim_y, 0.0f, 1.0f);
    shared.gun.trigger = trigger ? 1 : 0;
    shared.gun.offscreen = offscreen ? 1 : 0;
    shared.gun.coin = coin ? 1 : 0;
    shared.gun.start = start ? 1 : 0;

    sequence.fetch_add(1, std::memory_order_release);
}

FrameStatus read_frame_status(const MameSharedState& shared) noexcept {
    std::atomic_ref<std::uint64_t> sequence(const_cast<std::uint64_t&>(shared.frame.sequence));
    for (int attempt = 0; attempt < 4; ++attempt) {
        const std::uint64_t before = sequence.load(std::memory_order_acquire);
        if (before & 1u) {
            continue;
        }

        FrameStatus status{};
        if (shared.frame.protocol_version != kMameBridgeProtocolVersion) {
            return status;
        }
        status.frame_number = shared.frame.frame_number;
        status.width = shared.frame.width;
        status.height = shared.frame.height;
        status.payload_bytes = shared.frame.payload_bytes;

        const std::uint64_t after = sequence.load(std::memory_order_acquire);
        if (before == after && !(after & 1u)) {
            return status;
        }
    }
    return {};
}

}  // namespace area51xr
