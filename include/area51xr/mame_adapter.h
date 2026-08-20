#pragma once

#include "area51xr/mame_ipc.h"

#include <cstdint>
#include <span>

namespace area51xr {

class MameAdapter {
public:
    explicit MameAdapter(MameSharedState* shared) noexcept;

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] CoJagGunRegisters gun_registers() const noexcept;
    [[nodiscard]] bool trigger_down() const noexcept;

    bool submit_frame(
        std::uint64_t frame_number,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t stride_bytes,
        std::uint32_t pixel_format,
        std::uint64_t presentation_time_ns,
        std::span<const std::uint8_t> pixels) noexcept;

private:
    MameSharedState* shared_{};
};

}  // namespace area51xr
