#include "area51xr/mame_adapter.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>

int main() {
    auto shared = std::make_unique<area51xr::MameSharedState>();
    shared->protocol_version = area51xr::kMameBridgeProtocolVersion;
    shared->gun.protocol_version = area51xr::kMameBridgeProtocolVersion;
    shared->gun.aim_x = 0.5f;
    shared->gun.aim_y = 0.5f;
    shared->gun.trigger = 1;

    area51xr::MameAdapter adapter(shared.get());
    assert(adapter.connected());
    assert(adapter.trigger_down());

    const auto gun = adapter.gun_registers();
    assert(gun.x <= 0x1ff);
    assert(gun.y <= 0x1ff);

    const std::array<std::uint8_t, 4> pixels{10, 20, 30, 40};
    assert(adapter.submit_frame(9, 1, 1, 4, 1, 123456, pixels));
    assert(shared->frame.frame_number == 9);
    assert(shared->frame.width == 1);
    assert(shared->frame.height == 1);
    assert(shared->frame.payload_bytes == pixels.size());
    assert(shared->frame_pixels[0] == 10);
    assert(shared->frame_pixels[3] == 40);

    shared->protocol_version = 999;
    assert(!adapter.connected());
    assert(!adapter.submit_frame(10, 1, 1, 4, 1, 0, pixels));

    return 0;
}