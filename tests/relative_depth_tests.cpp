#include "area51xr/relative_depth.h"

#include <array>
#include <cassert>
#include <cmath>
#include <vector>

namespace {
bool near(float a, float b, float eps = 1.0e-4f) { return std::abs(a - b) < eps; }
}

int main() {
    const std::array<float, 4> disparity{1.0f, 2.0f, 3.0f, 4.0f};
    area51xr::RelativeDepthCalibration calibration{};
    calibration.near_distance_m = 1.0f;
    calibration.far_distance_m = 4.0f;
    calibration.far_percentile = 0.0f;
    calibration.near_percentile = 1.0f;

    std::vector<float> depth;
    area51xr::RelativeDepthStats stats{};
    assert(area51xr::relative_disparity_to_metric_depth(disparity, depth, stats, calibration));
    assert(depth.size() == disparity.size());
    assert(near(depth.front(), 4.0f));
    assert(near(depth.back(), 1.0f));
    assert(depth[1] < depth[0]);
    assert(depth[2] < depth[1]);
    assert(near(stats.far_disparity, 1.0f));
    assert(near(stats.near_disparity, 4.0f));

    const std::array<float, 4> invalid{0.0f, 1.0f, 2.0f, 4.0f};
    assert(area51xr::relative_disparity_to_metric_depth(invalid, depth, stats, calibration));
    assert(near(depth[0], 0.0f));
    assert(stats.valid_samples == 3);

    const std::array<float, 3> flat{2.0f, 2.0f, 2.0f};
    assert(!area51xr::relative_disparity_to_metric_depth(flat, depth, stats, calibration));
    return 0;
}
