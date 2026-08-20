#include "area51xr/depth_anything_v2.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace area51xr {
namespace {

float sample_bgra_channel(
    const ColorFrameView& frame,
    float source_x,
    float source_y,
    std::uint32_t channel) noexcept {
    source_x = std::clamp(source_x, 0.0f, static_cast<float>(frame.width - 1));
    source_y = std::clamp(source_y, 0.0f, static_cast<float>(frame.height - 1));
    const auto x0 = static_cast<std::uint32_t>(std::floor(source_x));
    const auto y0 = static_cast<std::uint32_t>(std::floor(source_y));
    const auto x1 = std::min(x0 + 1, frame.width - 1);
    const auto y1 = std::min(y0 + 1, frame.height - 1);
    const float tx = source_x - static_cast<float>(x0);
    const float ty = source_y - static_cast<float>(y0);

    const auto read = [&](std::uint32_t x, std::uint32_t y) {
        const std::size_t offset = static_cast<std::size_t>(y) * frame.stride_bytes +
                                   static_cast<std::size_t>(x) * 4u + channel;
        return static_cast<float>(frame.pixels[offset]);
    };
    const float top = read(x0, y0) + (read(x1, y0) - read(x0, y0)) * tx;
    const float bottom = read(x0, y1) + (read(x1, y1) - read(x0, y1)) * tx;
    return top + (bottom - top) * ty;
}

float sample_scalar(
    std::span<const float> field,
    std::uint32_t width,
    std::uint32_t height,
    float source_x,
    float source_y) noexcept {
    source_x = std::clamp(source_x, 0.0f, static_cast<float>(width - 1));
    source_y = std::clamp(source_y, 0.0f, static_cast<float>(height - 1));
    const auto x0 = static_cast<std::uint32_t>(std::floor(source_x));
    const auto y0 = static_cast<std::uint32_t>(std::floor(source_y));
    const auto x1 = std::min(x0 + 1, width - 1);
    const auto y1 = std::min(y0 + 1, height - 1);
    const float tx = source_x - static_cast<float>(x0);
    const float ty = source_y - static_cast<float>(y0);
    const auto at = [&](std::uint32_t x, std::uint32_t y) {
        return field[static_cast<std::size_t>(y) * width + x];
    };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return top + (bottom - top) * ty;
}

} // namespace

bool preprocess_depth_anything_v2(
    const ColorFrameView& frame,
    DepthAnythingTensor& output,
    std::uint32_t input_size) {
    output = {};
    const std::size_t pixel_bytes = static_cast<std::size_t>(frame.stride_bytes) * frame.height;
    if (frame.width == 0 || frame.height == 0 || frame.stride_bytes < frame.width * 4u ||
        frame.pixels.size() < pixel_bytes || input_size < 14 || input_size % 14 != 0) {
        return false;
    }

    output.width = input_size;
    output.height = input_size;
    const std::size_t plane_size = static_cast<std::size_t>(input_size) * input_size;
    output.chw_rgb.resize(plane_size * 3u);

    constexpr std::array<float, 3> mean{0.485f, 0.456f, 0.406f};
    constexpr std::array<float, 3> stddev{0.229f, 0.224f, 0.225f};
    constexpr std::array<std::uint32_t, 3> bgra_channel_for_rgb{2u, 1u, 0u};

    for (std::uint32_t y = 0; y < input_size; ++y) {
        const float source_y = (static_cast<float>(y) + 0.5f) *
            static_cast<float>(frame.height) / static_cast<float>(input_size) - 0.5f;
        for (std::uint32_t x = 0; x < input_size; ++x) {
            const float source_x = (static_cast<float>(x) + 0.5f) *
                static_cast<float>(frame.width) / static_cast<float>(input_size) - 0.5f;
            const std::size_t index = static_cast<std::size_t>(y) * input_size + x;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float value = sample_bgra_channel(
                    frame, source_x, source_y, bgra_channel_for_rgb[channel]) / 255.0f;
                output.chw_rgb[channel * plane_size + index] =
                    (value - mean[channel]) / stddev[channel];
            }
        }
    }
    return true;
}

bool depth_anything_v2_output_to_metric(
    std::span<const float> model_disparity,
    std::uint32_t model_width,
    std::uint32_t model_height,
    std::uint32_t output_width,
    std::uint32_t output_height,
    std::vector<float>& depth_m,
    RelativeDepthStats& stats,
    const RelativeDepthCalibration& calibration) {
    depth_m.clear();
    stats = {};
    if (model_width == 0 || model_height == 0 || output_width == 0 || output_height == 0 ||
        model_disparity.size() != static_cast<std::size_t>(model_width) * model_height) {
        return false;
    }

    std::vector<float> resized(static_cast<std::size_t>(output_width) * output_height);
    for (std::uint32_t y = 0; y < output_height; ++y) {
        const float source_y = (static_cast<float>(y) + 0.5f) *
            static_cast<float>(model_height) / static_cast<float>(output_height) - 0.5f;
        for (std::uint32_t x = 0; x < output_width; ++x) {
            const float source_x = (static_cast<float>(x) + 0.5f) *
                static_cast<float>(model_width) / static_cast<float>(output_width) - 0.5f;
            resized[static_cast<std::size_t>(y) * output_width + x] =
                sample_scalar(model_disparity, model_width, model_height, source_x, source_y);
        }
    }
    return relative_disparity_to_metric_depth(resized, depth_m, stats, calibration);
}

} // namespace area51xr
