#include "area51xr/depth_stabilizer.h"

#include <algorithm>
#include <cmath>

namespace area51xr {

DepthStabilizer::DepthStabilizer(DepthStabilizerOptions options) : options_(options) {}

bool DepthStabilizer::valid_depth(float value) const noexcept {
    return std::isfinite(value) && value >= options_.min_depth_m && value <= options_.max_depth_m;
}

StabilizedDepthFrame DepthStabilizer::update(std::span<const float> depth_m) {
    if (depth_.size() != depth_m.size()) {
        depth_.assign(depth_m.size(), 0.0f);
        confidence_.assign(depth_m.size(), 0.0f);
        initialized_ = false;
    }

    const float alpha = std::clamp(options_.smoothing_alpha, 0.0f, 1.0f);
    const float rise = std::clamp(options_.confidence_rise, 0.0f, 1.0f);
    const float decay = std::clamp(options_.confidence_decay, 0.0f, 1.0f);

    for (std::size_t i = 0; i < depth_m.size(); ++i) {
        const float incoming = depth_m[i];
        const bool incoming_valid = valid_depth(incoming);
        const bool stored_valid = valid_depth(depth_[i]);

        if (!incoming_valid) {
            confidence_[i] = std::max(0.0f, confidence_[i] - decay);
            if (confidence_[i] < options_.usable_confidence) {
                depth_[i] = 0.0f;
            }
            continue;
        }

        if (!initialized_ || !stored_valid) {
            depth_[i] = incoming;
            confidence_[i] = std::max(confidence_[i], rise);
            continue;
        }

        if (std::abs(incoming - depth_[i]) >= options_.discontinuity_reset_m) {
            // A large jump is more likely to be a newly revealed foreground/background
            // surface than estimator jitter. Snap instead of leaving a temporal trail.
            depth_[i] = incoming;
            confidence_[i] = rise;
            continue;
        }

        depth_[i] += alpha * (incoming - depth_[i]);
        confidence_[i] = std::min(1.0f, confidence_[i] + rise);
    }

    initialized_ = true;
    return {depth_, confidence_};
}

void DepthStabilizer::reset() {
    depth_.clear();
    confidence_.clear();
    initialized_ = false;
}

std::size_t DepthStabilizer::size() const noexcept {
    return depth_.size();
}

const DepthStabilizerOptions& DepthStabilizer::options() const noexcept {
    return options_;
}

} // namespace area51xr
