#include "area51xr/reconstruction_capture.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> make_bgra_gradient(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4u);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
            pixels[offset + 0] = static_cast<std::uint8_t>((x * 255u) / (width > 1 ? width - 1 : 1));
            pixels[offset + 1] = static_cast<std::uint8_t>((y * 255u) / (height > 1 ? height - 1 : 1));
            pixels[offset + 2] = 160;
            pixels[offset + 3] = 255;
        }
    }
    return pixels;
}

std::vector<float> make_depth_field(std::uint32_t width, std::uint32_t height) {
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const float nx = width > 1 ? static_cast<float>(x) / static_cast<float>(width - 1) : 0.0f;
            const float ny = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
            depth[static_cast<std::size_t>(y) * width + x] = 1.4f + 1.2f * nx + 0.35f * ny;
        }
    }
    return depth;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: area51xr-generate-capture <output.a51cap>\n";
        return 1;
    }

    constexpr std::uint32_t width = 64;
    constexpr std::uint32_t height = 48;
    auto pixels = make_bgra_gradient(width, height);
    auto depth = make_depth_field(width, height);

    area51xr::ColorFrameView frame{};
    frame.frame_number = 1;
    frame.width = width;
    frame.height = height;
    frame.stride_bytes = width * 4u;
    frame.pixels = pixels;

    const auto encoded = area51xr::encode_reconstruction_capture(frame, depth);
    if (encoded.empty()) {
        std::cerr << "failed to encode synthetic capture\n";
        return 2;
    }

    const std::filesystem::path output = argv[1];
    std::filesystem::create_directories(output.parent_path().empty() ? "." : output.parent_path());
    std::ofstream out(output, std::ios::binary);
    if (!out) {
        std::cerr << "failed to open output capture: " << output.string() << '\n';
        return 3;
    }
    out.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    if (!out) {
        std::cerr << "failed to write output capture: " << output.string() << '\n';
        return 4;
    }

    std::cout << "capture=" << output.string()
              << " size=" << width << 'x' << height
              << " bytes=" << encoded.size() << '\n';
    return 0;
}
