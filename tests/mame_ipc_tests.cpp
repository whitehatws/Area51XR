#include "area51xr/mame_ipc.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>

int main() {
    // MameSharedState contains the full shared framebuffer and is larger than
    // the default Windows process stack. Keep the test fixture on the heap.
    auto shared = std::make_unique<area51xr::MameSharedState>();

    area51xr::MameFrameHeader header{};
    header.frame_number = 42;
    header.width = 2;
    header.height = 1;
    header.stride_bytes = 8;
    header.pixel_format = 1;

    const std::array<std::uint8_t, 8> pixels{1, 2, 3, 4, 5, 6, 7, 8};
    header.payload_bytes = pixels.size();

    assert(area51xr::publish_frame(*shared, header, pixels));
    assert(shared->frame.frame_number == 42);
    assert((shared->frame.sequence & 1u) == 0);

    std::array<std::uint8_t, 8> copied{};
    area51xr::MameFrameHeader copied_header{};
    assert(area51xr::copy_latest_frame(*shared, copied_header, copied));
    assert(copied_header.frame_number == 42);
    assert(copied_header.width == 2);
    assert(copied_header.height == 1);
    assert(copied == pixels);

    auto bad_header = header;
    bad_header.payload_bytes = pixels.size() + 1;
    assert(!area51xr::publish_frame(*shared, bad_header, pixels));

    std::array<std::uint8_t, 4> too_small{};
    assert(!area51xr::copy_latest_frame(*shared, copied_header, too_small));

#ifdef _WIN32
    area51xr::MameIpc owner;
    assert(owner.create());
    assert(owner.valid());

    owner.state()->gun.sequence = 8;
    owner.state()->gun.aim_x = 0.25f;
    owner.state()->gun.aim_y = 0.75f;

    area51xr::MameIpc client;
    assert(client.open());
    assert(client.valid());
    assert(client.state()->gun.sequence == 8);
    assert(client.state()->gun.aim_x == 0.25f);
    assert(client.state()->gun.aim_y == 0.75f);
#endif

    return 0;
}
