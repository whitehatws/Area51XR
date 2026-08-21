#pragma once

#include "area51xr/depth_anything_v2.h"
#include "area51xr/depth_provider.h"

#include <cstdint>
#include <span>
#include <vector>

namespace area51xr {

class TensorInferenceBackend {
public:
    virtual ~TensorInferenceBackend() = default;

    virtual bool run(
        std::span<const float> chw_rgb,
        std::uint32_t input_width,
        std::uint32_t input_height,
        std::vector<float>& output,
        std::uint32_t& output_width,
        std::uint32_t& output_height) = 0;
};

class DepthAnythingV2Provider final : public DepthProvider {
public:
    DepthAnythingV2Provider(
        TensorInferenceBackend& backend,
        std::uint32_t input_size = 518,
        RelativeDepthCalibration calibration = {},
        RelativeDepthAnchorOptions anchor_options = {});

    bool estimate(const ColorFrameView& frame, DepthEstimate& output) override;

    [[nodiscard]] const RelativeDepthStats& last_stats() const noexcept;
    [[nodiscard]] const RelativeDepthStats& anchors() const noexcept;
    void reset_anchors() noexcept;

private:
    TensorInferenceBackend& backend_;
    std::uint32_t input_size_{};
    RelativeDepthCalibration calibration_{};
    RelativeDepthAnchorTracker anchor_tracker_;
    DepthAnythingTensor input_{};
    std::vector<float> raw_output_;
    RelativeDepthStats stats_{};
};

} // namespace area51xr
