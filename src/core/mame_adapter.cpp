#include "area51xr/mame_adapter.h"

namespace area51xr {

MameAdapter::MameAdapter(MameSharedState* shared) noexcept : shared_(shared) {}

bool MameAdapter::connected() const noexcept {
    return shared_ && shared_->protocol_version == kMameBridgeProtocolVersion;
}

CoJagGunRegisters MameAdapter::gun_registers() const noexcept {
    if (!connected()) {
        return {};
    }
    return encode_cojag_gun(shared_->gun.aim_x, shared_->gun.aim_y);
}

bool MameAdapter::trigger_down() const noexcept {
    return connected() && shared_->gun.trigger != 0;
}

bool MameAdapter::submit_frame(
    std::uint64_t frame_number,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t stride_bytes,
    std::uint32_t pixel_format,
    std::uint64_t presentation_time_ns,
    std::span<const std::uint8_t> pixels) noexcept {
    if (!connected()) {
        return false;
    }

    MameFrameHeader header{};
    header.frame_number = frame_number;
    header.width = width;
    header.height = height;
    header.stride_bytes = stride_bytes;
    header.pixel_format = pixel_format;
    header.presentation_time_ns = presentation_time_ns;
    header.payload_bytes = pixels.size();
    return publish_frame(*shared_, header, pixels);
}

}  // namespace area51xr
