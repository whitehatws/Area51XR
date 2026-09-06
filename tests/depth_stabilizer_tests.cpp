#include "area51xr/depth_stabilizer.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {
bool near(float a, float b, float eps = 1.0e-4f) { return std::abs(a - b) < eps; }
}

int main() {
    area51xr::DepthStabilizerOptions options{};
    options.smoothing_alpha = 0.25f;
    options.discontinuity_reset_m = 0.75f;
    options.confidence_rise = 0.25f;
    options.confidence_decay = 0.25f;
    options.usable_confidence = 0.25f;

    area51xr::DepthStabilizer stabilizer(options);

    const std::array<float, 2> first{2.0f, 4.0f};
    auto frame = stabilizer.update(first);
    assert(frame.depth_m.size() == 2);
    assert(near(frame.depth_m[0], 2.0f));
    assert(near(frame.depth_m[1], 4.0f));
    assert(near(frame.confidence[0], 0.25f));

    // Small changes are temporally smoothed.
    const std::array<float, 2> jitter{2.4f, 3.6f};
    frame = stabilizer.update(jitter);
    assert(near(frame.depth_m[0], 2.1f));
    assert(near(frame.depth_m[1], 3.9f));
    assert(near(frame.confidence[0], 0.5f));

    // A foreground/background transition snaps immediately rather than smearing.
    const std::array<float, 2> jump{0.8f, 8.0f};
    frame = stabilizer.update(jump);
    assert(near(frame.depth_m[0], 0.8f));
    assert(near(frame.depth_m[1], 8.0f));
    assert(near(frame.confidence[0], 0.25f));

    // Missing depth decays confidence and eventually invalidates the sample.
    const std::array<float, 2> missing{0.0f, 8.0f};
    frame = stabilizer.update(missing);
    assert(near(frame.confidence[0], 0.0f));
    assert(near(frame.depth_m[0], 0.0f));

    // Reappearing valid depth is accepted immediately and begins rebuilding confidence.
    const std::array<float, 2> recovered{1.1f, 8.0f};
    frame = stabilizer.update(recovered);
    assert(near(frame.depth_m[0], 1.1f));
    assert(near(frame.confidence[0], 0.25f));

    stabilizer.reset();
    assert(stabilizer.size() == 0);
    return 0;
}
