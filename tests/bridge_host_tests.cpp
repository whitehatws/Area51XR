#include "area51xr/bridge_host.h"

#include <cassert>

int main() {
    area51xr::MameSharedState shared{};

    area51xr::write_gun_state(shared, 0.25f, 0.75f, true);
    assert(shared.gun.protocol_version == area51xr::kMameBridgeProtocolVersion);
    assert(shared.gun.aim_x == 0.25f);
    assert(shared.gun.aim_y == 0.75f);
    assert(shared.gun.trigger == 1);
    assert(shared.gun.sequence == 2);

    area51xr::write_gun_state(shared, -1.0f, 2.0f, false);
    assert(shared.gun.aim_x == 0.0f);
    assert(shared.gun.aim_y == 1.0f);
    assert(shared.gun.trigger == 0);
    assert(shared.gun.sequence == 4);

    shared.frame.protocol_version = area51xr::kMameBridgeProtocolVersion;
    shared.frame.frame_number = 12;
    shared.frame.width = 760;
    shared.frame.height = 512;
    shared.frame.payload_bytes = 760u * 512u * 4u;

    const auto status = area51xr::read_frame_status(shared);
    assert(status.frame_number == 12);
    assert(status.width == 760);
    assert(status.height == 512);
    assert(status.payload_bytes == 760u * 512u * 4u);

    return 0;
}
