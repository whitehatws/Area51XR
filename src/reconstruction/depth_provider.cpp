#include "area51xr/depth_provider.h"

#include <algorithm>
#include <cmath>

namespace area51xr {

SyntheticDepthProvider::SyntheticDepthProvider(float base_depth_m, float horizontal_slope_m)
    : base_depth_m_(base_depth_m), horizontal_slope_m_(horizontal_slope_m) {}

bool SyntheticDepthProvider::estimate(const ColorFrameView& frame, DepthEstimate& output) {
    if (frame.width == 0 || frame.height == 0 ||
        frame.stride_bytes < frame.width * 4u ||
        frame.pixels.size() < static_cast<std::size_t>(frame.stride_bytes) * frame.height) {
        output = {};
        return false;
    }

    output.width = frame.width;
    output.height = frame.height;
    output.depth_m.resize(static_cast<std::size_t>(frame.width) * frame.height);

    const float denominator = frame.width > 1 ? static_cast<float>(frame.width - 1) : 1.0f;
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        for (std::uint32_t x = 0; x < frame.width; ++x) {
            const float normalized_x = static_cast<float>(x) / denominator - 0.5f;
            const float depth = std::max(0.01f, base_depth_m_ + horizontal_slope_m_ * normalized_x);
            output.depth_m[static_cast<std::size_t>(y) * frame.width + x] = depth;
        }
    }
    return true;
}

void SyntheticDepthProvider::set_base_depth(float depth_m) noexcept {
    if (std::isfinite(depth_m)) {
        base_depth_m_ = depth_m;
    }
}

void SyntheticDepthProvider::set_horizontal_slope(float slope_m) noexcept {
    if (std::isfinite(slope_m)) {
        horizontal_slope_m_ = slope_m;
    }
}

} // namespace area51xr
