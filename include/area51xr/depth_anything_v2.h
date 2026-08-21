#pragma once

#include "area51xr/depth_provider.h"
#include "area51xr/relative_depth.h"

#include <cstdint>
#include <span>
#include <vector>

namespace area51xr {

struct DepthAnythingTensor {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<float> chw_rgb;
};

bool preprocess_depth_anything_v2(
    const ColorFrameView& frame,
    DepthAnythingTensor& output,
    std::uint32_t input_size = 518);

bool depth_anything_v2_output_to_metric(
    std::span<const float> model_disparity,
    std::uint32_t model_width,
    std::uint32_t model_height,
    std::uint32_t output_width,
    std::uint32_t output_height,
    std::vector<float>& depth_m,
    RelativeDepthStats& stats,
    const RelativeDepthCalibration& calibration = {});

bool depth_anything_v2_output_to_metric_stabilized(
    std::span<const float> model_disparity,
    std::uint32_t model_width,
    std::uint32_t model_height,
    std::uint32_t output_width,
    std::uint32_t output_height,
    std::vector<float>& depth_m,
    RelativeDepthStats& anchors_used,
    RelativeDepthAnchorTracker& anchor_tracker,
    const RelativeDepthCalibration& calibration = {});

} // namespace area51xr
