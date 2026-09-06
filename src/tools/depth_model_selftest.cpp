#include "area51xr/depth_anything_v2.h"
#include "area51xr/onnx_depth_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: area51xr-depth-selftest <depth_anything_v2_vits.onnx>\n";
        return 1;
    }

    const std::filesystem::path model = argv[1];
    area51xr::OnnxRuntimeBackend backend(model);
    if (!backend.valid()) {
        std::cerr << "failed to load ONNX model: " << backend.last_error() << '\n';
        return 2;
    }

    constexpr std::uint32_t width = 320;
    constexpr std::uint32_t height = 240;
    constexpr std::uint32_t stride = width * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y) * stride + x * 4u;
            pixels[offset + 0] = static_cast<std::uint8_t>((x * 255u) / (width - 1));
            pixels[offset + 1] = static_cast<std::uint8_t>((y * 255u) / (height - 1));
            pixels[offset + 2] = static_cast<std::uint8_t>(((x ^ y) * 255u) / 511u);
            pixels[offset + 3] = 255;
        }
    }

    area51xr::ColorFrameView frame{};
    frame.width = width;
    frame.height = height;
    frame.stride_bytes = stride;
    frame.pixels = pixels;

    area51xr::DepthAnythingTensor input{};
    if (!area51xr::preprocess_depth_anything_v2(frame, input)) {
        std::cerr << "Depth Anything preprocessing failed\n";
        return 3;
    }

    std::vector<float> output;
    std::uint32_t output_width{};
    std::uint32_t output_height{};
    if (!backend.run(
            input.chw_rgb,
            input.width,
            input.height,
            output,
            output_width,
            output_height)) {
        std::cerr << "ONNX inference failed: " << backend.last_error() << '\n';
        return 4;
    }
    if (output.empty() || output_width == 0 || output_height == 0 ||
        output.size() != static_cast<std::size_t>(output_width) * output_height) {
        std::cerr << "ONNX model returned invalid output dimensions\n";
        return 5;
    }

    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    std::size_t finite_count{};
    for (const float value : output) {
        if (std::isfinite(value)) {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            ++finite_count;
        }
    }
    if (finite_count != output.size() || maximum - minimum <= 1.0e-6f) {
        std::cerr << "ONNX depth output is non-finite or flat\n";
        return 6;
    }

    std::cout << "DEPTH MODEL SELF-TEST: PASS\n"
              << "input=" << input.width << 'x' << input.height << '\n'
              << "output=" << output_width << 'x' << output_height << '\n'
              << "range=" << minimum << ".." << maximum << '\n';
    return 0;
}
