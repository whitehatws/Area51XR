#pragma once

#include "area51xr/mame_bridge.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace area51xr {

constexpr std::size_t kMameFrameBufferBytes = 760u * 512u * 4u;

struct MameSharedState {
    std::uint32_t protocol_version{kMameBridgeProtocolVersion};
    std::uint32_t reserved0{};
    MameGunState gun{};
    MameFrameHeader frame{};
    std::uint8_t frame_pixels[kMameFrameBufferBytes]{};
};

static_assert(sizeof(MameGunState) == 32);
static_assert(sizeof(MameFrameHeader) == 56);
static_assert(offsetof(MameSharedState, gun) == 8);
static_assert(offsetof(MameSharedState, frame) == 40);
static_assert(offsetof(MameSharedState, frame_pixels) == 96);
static_assert(sizeof(MameSharedState::frame_pixels) == kMameFrameBufferBytes);

class MameIpc {
public:
    MameIpc() = default;
    ~MameIpc();

    MameIpc(const MameIpc&) = delete;
    MameIpc& operator=(const MameIpc&) = delete;

    bool create();
    bool open();
    void close();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] MameSharedState* state() noexcept;
    [[nodiscard]] const MameSharedState* state() const noexcept;

private:
    void* mapping_{};
    MameSharedState* state_{};
};

bool publish_frame(
    MameSharedState& shared,
    const MameFrameHeader& header,
    std::span<const std::uint8_t> pixels);

bool copy_latest_frame(
    const MameSharedState& shared,
    MameFrameHeader& header,
    std::span<std::uint8_t> destination) noexcept;

}  // namespace area51xr
