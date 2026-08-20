#pragma once

#include <cstdint>

namespace area51xr {

constexpr std::uint32_t kMameBridgeProtocolVersion = 1;

struct MameGunState {
    std::uint32_t protocol_version{kMameBridgeProtocolVersion};
    std::uint64_t sequence{};
    float aim_x{};
    float aim_y{};
    std::uint8_t trigger{};
    std::uint8_t reserved[7]{};
};

struct MameFrameHeader {
    std::uint32_t protocol_version{kMameBridgeProtocolVersion};
    std::uint64_t frame_number{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride_bytes{};
    std::uint32_t pixel_format{};
    std::uint64_t presentation_time_ns{};
    std::uint64_t payload_bytes{};
};

struct CoJagGunRegisters {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint32_t packed{};
};

CoJagGunRegisters encode_cojag_gun(float normalized_x, float normalized_y);

}  // namespace area51xr
