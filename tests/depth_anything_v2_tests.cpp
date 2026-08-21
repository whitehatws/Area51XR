#include "area51xr/depth_anything_v2.h"

#include <array>
#include <cassert>
#include <cmath>
#include <vector>

namespace {
bool near(float a, float b, float eps = 1.0e-3f) { return std::abs(a - b) < eps; }
}

int main() {
    const std::array<std::uint8_t, 4> pixel{0, 127, 255, 255}; // BGRA
    area51xr::ColorFrameView frame{};
    frame.width = 1;
    frame.height = 1;
    frame.stride_bytes = 4;
    frame.pixels = pixel;

    area51xr::DepthAnythingTensor tensor{};
    assert(area51xr::preprocess_depth_anything_v2(frame, tensor, 14));
    assert(tensor.width == 14);
    assert(tensor.height == 14);
    assert(tensor.chw_rgb.size() == 14u * 14u * 3u);

    const std::size_t plane = 14u * 14u;
    const float expected_r = (1.0f - 0.485f) / 0.229f;
    const float expected_g = ((127.0f / 255.0f) - 0.456f) / 0.224f;
    const float expected_b = (0.0f - 0.406f) / 0.225f;
    assert(near(tensor.chw_rgb[0], expected_r));
    assert(near(tensor.chw_rgb[plane], expected_g));
    assert(near(tensor.chw_rgb[plane * 2], expected_b));
    assert(!area51xr::preprocess_depth_anything_v2(frame, tensor, 15));

    const std::array<float, 4> model_output{1.0f, 2.0f, 3.0f, 4.0f};
    area51xr::RelativeDepthCalibration calibration{};
    calibration.near_distance_m = 1.0f;
    calibration.far_distance_m = 4.0f;
    calibration.far_percentile = 0.0f;
    calibration.near_percentile = 1.0f;

    std::vector<float> depth;
    area51xr::RelativeDepthStats stats{};
    assert(area51xr::depth_anything_v2_output_to_metric(
        model_output, 2, 2, 2, 2, depth, stats, calibration));
    assert(depth.size() == 4);
    assert(near(depth[0], 4.0f));
    assert(near(depth[3], 1.0f));
    return 0;
}
