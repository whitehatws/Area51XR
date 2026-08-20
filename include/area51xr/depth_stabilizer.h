#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace area51xr {

struct DepthStabilizerOptions {
    float smoothing_alpha{0.18f};
    float discontinuity_reset_m{0.75f};
    float min_depth_m{0.1f};
    float max_depth_m{20.0f};
    float confidence_rise{0.2f};
    float confidence_decay{0.35f};
    float usable_confidence{0.35f};
};

struct StabilizedDepthFrame {
    std::span<const float> depth_m;
    std::span<const float> confidence;
};

class DepthStabilizer {
public:
    explicit DepthStabilizer(DepthStabilizerOptions options = {});

    StabilizedDepthFrame update(std::span<const float> depth_m);
    void reset();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const DepthStabilizerOptions& options() const noexcept;

private:
    bool valid_depth(float value) const noexcept;

    DepthStabilizerOptions options_{};
    std::vector<float> depth_;
    std::vector<float> confidence_;
    bool initialized_{};
};

} // namespace area51xr
