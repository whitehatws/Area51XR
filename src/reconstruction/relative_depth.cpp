#include "area51xr/relative_depth.h"

#include <algorithm>
#include <cmath>

namespace area51xr {
namespace {

float percentile(std::vector<float>& values, float p) {
    if (values.empty()) return 0.0f;
    p = std::clamp(p, 0.0f, 1.0f);
    const float position = p * static_cast<float>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(lower), values.end());
    const float a = values[lower];
    if (upper == lower) return a;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(upper), values.end());
    const float b = values[upper];
    return a + (b - a) * (position - static_cast<float>(lower));
}

} // namespace

bool relative_disparity_to_metric_depth(
    std::span<const float> disparity,
    std::vector<float>& depth_m,
    RelativeDepthStats& stats,
    const RelativeDepthCalibration& calibration) {
    stats = {};
    depth_m.assign(disparity.size(), 0.0f);
    if (disparity.empty() || calibration.near_distance_m <= 0.0f ||
        calibration.far_distance_m <= calibration.near_distance_m ||
        calibration.far_percentile < 0.0f || calibration.near_percentile > 1.0f ||
        calibration.far_percentile >= calibration.near_percentile) {
        return false;
    }

    std::vector<float> valid;
    valid.reserve(disparity.size());
    for (const float value : disparity) {
        if (std::isfinite(value) && value > calibration.min_valid_value) {
            valid.push_back(value);
        }
    }
    if (valid.size() < 2) {
        return false;
    }

    auto work = valid;
    const float far_disparity = percentile(work, calibration.far_percentile);
    work = valid;
    const float near_disparity = percentile(work, calibration.near_percentile);
    if (!std::isfinite(far_disparity) || !std::isfinite(near_disparity) ||
        near_disparity - far_disparity <= 1.0e-6f) {
        return false;
    }

    stats.far_disparity = far_disparity;
    stats.near_disparity = near_disparity;
    stats.valid_samples = valid.size();

    const float near_inverse = 1.0f / calibration.near_distance_m;
    const float far_inverse = 1.0f / calibration.far_distance_m;
    const float disparity_range = near_disparity - far_disparity;

    for (std::size_t i = 0; i < disparity.size(); ++i) {
        const float value = disparity[i];
        if (!std::isfinite(value) || value <= calibration.min_valid_value) {
            depth_m[i] = 0.0f;
            continue;
        }
        const float normalized = std::clamp((value - far_disparity) / disparity_range, 0.0f, 1.0f);
        const float inverse_depth = far_inverse + normalized * (near_inverse - far_inverse);
        depth_m[i] = 1.0f / inverse_depth;
    }
    return true;
}

} // namespace area51xr
