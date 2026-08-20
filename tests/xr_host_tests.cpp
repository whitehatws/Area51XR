#include "area51xr/xr_host.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {
bool near(float a, float b) { return std::abs(a - b) < 1.0e-4f; }
}

int main() {
    area51xr::MameSharedState shared{};
    shared.protocol_version = area51xr::kMameBridgeProtocolVersion;

    area51xr::MameFrameHeader frame{};
    frame.frame_number = 12;
    frame.width = 1;
    frame.height = 1;
    frame.stride_bytes = 4;
    frame.pixel_format = 1;
    const std::array<std::uint8_t, 4> pixel{10, 20, 30, 255};
    frame.payload_bytes = pixel.size();
    assert(area51xr::publish_frame(shared, frame, pixel));

    area51xr::XrInputState input{};
    input.session_running = true;
    input.pose_valid = true;
    input.aim.position = {0.0f, 0.0f, 0.0f};
    input.trigger_down = true;

    area51xr::SimulatedXrRuntime runtime(input);
    area51xr::XrHost host(runtime, shared);
    assert(host.initialize());

    const auto first = host.tick();
    assert(first.runtime_ok);
    assert(first.session_running);
    assert(first.aim_valid);
    assert(near(first.aim.x, 0.5f));
    assert(near(first.aim.y, 0.5f));
    assert(first.trigger_down);
    assert(first.frame.frame_number == 12);
    assert(first.frame_presented);
    assert(runtime.last_presented_frame() == 12);
    assert(shared.gun.trigger == 1);
    assert(near(shared.gun.aim_x, 0.5f));
    assert(near(shared.gun.aim_y, 0.5f));

    input.pose_valid = false;
    input.trigger_down = true;
    runtime.set_state(input);
    const auto invalid = host.tick();
    assert(!invalid.aim_valid);
    assert(invalid.frame_presented);
    assert(runtime.last_presented_frame() == 12);
    assert(shared.gun.trigger == 0);
    assert(near(shared.gun.aim_x, 0.5f));
    assert(near(shared.gun.aim_y, 0.5f));

    host.shutdown();
    return 0;
}
