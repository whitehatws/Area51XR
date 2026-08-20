#include "area51xr/xr_runtime.h"

#include <cassert>
#include <cmath>

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 1.0e-4f;
}

} // namespace

int main() {
    area51xr::Pose identity{};
    const auto forward = area51xr::aim_forward(identity);
    assert(near(forward.x, 0.0f));
    assert(near(forward.y, 0.0f));
    assert(near(forward.z, -1.0f));

    const float s = std::sqrt(0.5f);
    area51xr::Pose right{};
    right.orientation = {0.0f, -s, 0.0f, s};
    const auto right_forward = area51xr::aim_forward(right);
    assert(near(right_forward.x, 1.0f));
    assert(near(right_forward.y, 0.0f));
    assert(near(right_forward.z, 0.0f));

    area51xr::XrInputState initial{};
    initial.session_running = true;
    initial.pose_valid = true;
    initial.sample_number = 7;
    area51xr::SimulatedXrRuntime runtime(initial);
    assert(runtime.initialize());

    area51xr::XrInputState sampled{};
    assert(runtime.poll(sampled));
    assert(sampled.sample_number == 7);
    assert(sampled.session_running);
    assert(sampled.pose_valid);

    runtime.shutdown();
    assert(!runtime.poll(sampled));
    return 0;
}
