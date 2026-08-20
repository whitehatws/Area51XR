#include "area51xr/mame_ipc.h"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    area51xr::MameSharedState shared{};

    area51xr::MameFrameHeader header{};
    header.frame_number = 42;
    header.width = 2;
    header.height = 1;
    header.stride_bytes = 8;
    header.pixel_format = 1;

    const std::array<std::uint8_t, 8> pixels{1, 2, 3, 4, 5, 6, 7, 8};
    header.payload_bytes = pixels.size();

    assert(area51xr::publish_frame(shared, header, pixels));
    assert(shared.frame.frame_number == 42);
    assert(shared.frame.width == 2);
    assert(shared.frame.height == 1);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        assert(shared.frame_pixels[i] == pixels[i]);
    }

    auto bad_header = header;
    bad_header.payload_bytes = pixels.size() + 1;
    assert(!area51xr::publish_frame(shared, bad_header, pixels));

#ifdef _WIN32
    area51xr::MameIpc owner;
    assert(owner.create());
    assert(owner.valid());

    owner.state()->gun.sequence = 7;
    owner.state()->gun.aim_x = 0.25f;
    owner.state()->gun.aim_y = 0.75f;

    area51xr::MameIpc client;
    assert(client.open());
    assert(client.valid());
    assert(client.state()->gun.sequence == 7);
    assert(client.state()->gun.aim_x == 0.25f);
    assert(client.state()->gun.aim_y == 0.75f);
#endif

    return 0;
}
