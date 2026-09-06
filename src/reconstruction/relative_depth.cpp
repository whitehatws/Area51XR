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

bool valid_anchors(const RelativeDepthStats& stats) noexcept {
    return stats.valid_samples >= 2 &&
           std::isfinite(stats.far_disparity) &&
           std::isfinite(stats.near_disparity) &&
           stats.near_disparity - stats.far_disparity > 1.0e-6f;
}

float smooth(float previous, float next, float alpha) noexcept {
    return previous + (next - previous) * alpha;
}

} // namespace

RelativeDepthAnchorTracker::RelativeDepthAnchorTracker(RelativeDepthAnchorOptions options)
    : options_(options) {}

RelativeDepthStats RelativeDepthAnchorTracker::update(const RelativeDepthStats& candidate) noexcept {
    if (!valid_anchors(candidate)) {
        return anchors_;
    }

    const float alpha = std::clamp(options_.smoothing_alpha, 0.0f, 1.0f);
    const float reset_multiple = std::max(options_.reset_span_multiple, 0.0f);

    if (!initialized_) {
        anchors_ = candidate;
        initialized_ = true;
        return anchors_;
    }

    const float current_span = std::max(anchors_.near_disparity - anchors_.far_disparity, 1.0e-6f);
    const float far_delta = std::abs(candidate.far_disparity - anchors_.far_disparity);
    const float near_delta = std::abs(candidate.near_disparity - anchors_.near_disparity);
    if (std::max(far_delta, near_delta) > current_span * reset_multiple) {
        anchors_ = candidate;
        return anchors_;
    }

    anchors_.far_disparity = smooth(anchors_.far_disparity, candidate.far_disparity, alpha);
    anchors_.near_disparity = smooth(anchors_.near_disparity, candidate.near_disparity, alpha);
    anchors_.valid_samples = candidate.valid_samples;
    if (!valid_anchors(anchors_)) {
        anchors_ = candidate;
    }
    return anchors_;
}

void RelativeDepthAnchorTracker::reset() noexcept {
    anchors_ = {};
    initialized_ = false;
}

bool RelativeDepthAnchorTracker::initialized() const noexcept {
    return initialized_;
}

const RelativeDepthStats& RelativeDepthAnchorTracker::anchors() const noexcept {
    return anchors_;
}

bool analyze_relative_disparity(
    std::span<const float> disparity,
    RelativeDepthStats& stats,
    const RelativeDepthCalibration& calibration) {
    stats = {};
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
    return true;
}

bool map_relative_disparity_to_metric_depth(
    std::span<const float> disparity,
    std::vector<float>& depth_m,
    const RelativeDepthStats& anchors,
    const RelativeDepthCalibration& calibration) {
    depth_m.assign(disparity.size(), 0.0f);
    if (!valid_anchors(anchors) || calibration.near_distance_m <= 0.0f ||
        calibration.far_distance_m <= calibration.near_distance_m) {
        return false;
    }

    const float near_inverse = 1.0f / calibration.near_distance_m;
    const float far_inverse = 1.0f / calibration.far_distance_m;
    const float disparity_range = anchors.near_disparity - anchors.far_disparity;

    for (std::size_t i = 0; i < disparity.size(); ++i) {
        const float value = disparity[i];
        if (!std::isfinite(value) || value <= calibration.min_valid_value) {
            depth_m[i] = 0.0f;
            continue;
        }
        const float normalized = std::clamp((value - anchors.far_disparity) / disparity_range, 0.0f, 1.0f);
        const float inverse_depth = far_inverse + normalized * (near_inverse - far_inverse);
        depth_m[i] = 1.0f / inverse_depth;
    }
    return true;
}

bool relative_disparity_to_metric_depth(
    std::span<const float> disparity,
    std::vector<float>& depth_m,
    RelativeDepthStats& stats,
    const RelativeDepthCalibration& calibration) {
    depth_m.assign(disparity.size(), 0.0f);
    if (!analyze_relative_disparity(disparity, stats, calibration)) {
        return false;
    }
    return map_relative_disparity_to_metric_depth(disparity, depth_m, stats, calibration);
}

} // namespace area51xr
