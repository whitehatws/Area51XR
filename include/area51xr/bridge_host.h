#pragma once

#include "area51xr/mame_ipc.h"

#include <cstdint>

namespace area51xr {

void write_gun_state(
    MameSharedState& shared,
    float aim_x,
    float aim_y,
    bool trigger,
    bool offscreen = false,
    bool coin = false,
    bool start = false,
    bool pause = false,
    std::uint8_t restart_token = 0,
    std::uint8_t quit_token = 0) noexcept;

struct FrameStatus {
    std::uint64_t frame_number{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint64_t payload_bytes{};
};

FrameStatus read_frame_status(const MameSharedState& shared) noexcept;

}  // namespace area51xr
