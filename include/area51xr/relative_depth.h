#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace area51xr {

struct RelativeDepthCalibration {
    float near_distance_m{0.75f};
    float far_distance_m{6.0f};
    float far_percentile{0.05f};
    float near_percentile{0.95f};
    float min_valid_value{1.0e-6f};
};

struct RelativeDepthStats {
    float far_disparity{};
    float near_disparity{};
    std::size_t valid_samples{};
};

bool relative_disparity_to_metric_depth(
    std::span<const float> disparity,
    std::vector<float>& depth_m,
    RelativeDepthStats& stats,
    const RelativeDepthCalibration& calibration = {});

} // namespace area51xr
