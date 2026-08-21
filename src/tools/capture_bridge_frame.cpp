#include "area51xr/depth_provider.h"
#include "area51xr/mame_ipc.h"
#include "area51xr/reconstruction_capture.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

bool write_file(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

void print_usage() {
    std::cerr << "usage: area51xr-capture <output.a51cap> [timeout-ms]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        print_usage();
        return 1;
    }

    const std::filesystem::path output_path = argv[1];
    int timeout_ms = 10000;
    if (argc == 3) {
        try {
            timeout_ms = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "invalid timeout\n";
            return 1;
        }
        if (timeout_ms < 100) {
            std::cerr << "timeout must be at least 100 ms\n";
            return 1;
        }
    }

    area51xr::MameIpc ipc;
    if (!ipc.open()) {
        std::cerr << "MAME bridge is not available. Start Area51XR host before capturing.\n";
        return 2;
    }

    std::vector<std::uint8_t> pixels(area51xr::kMameFrameBufferBytes);
    area51xr::MameFrameHeader header{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (area51xr::copy_latest_frame(*ipc.state(), header, pixels) && header.frame_number != 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    if (header.frame_number == 0 || header.payload_bytes == 0) {
        std::cerr << "timed out waiting for a MAME framebuffer\n";
        return 3;
    }

    area51xr::ColorFrameView frame{};
    frame.frame_number = header.frame_number;
    frame.width = header.width;
    frame.height = header.height;
    frame.stride_bytes = header.stride_bytes;
    frame.pixels = std::span<const std::uint8_t>(
        pixels.data(), static_cast<std::size_t>(header.payload_bytes));

    // This capture tool deliberately uses a deterministic synthetic depth map.
    // Model-backed captures can be added later without changing the .a51cap format.
    area51xr::SyntheticDepthProvider depth_provider(2.0f, 0.25f);
    area51xr::DepthEstimate depth{};
    if (!depth_provider.estimate(frame, depth)) {
        std::cerr << "failed to estimate capture depth\n";
        return 4;
    }

    const auto bytes = area51xr::encode_reconstruction_capture(frame, depth.depth_m);
    if (bytes.empty()) {
        std::cerr << "failed to encode reconstruction capture\n";
        return 5;
    }

    if (!write_file(output_path, bytes)) {
        std::cerr << "failed to write capture: " << output_path.string() << '\n';
        return 6;
    }

    std::cout << "capture=" << output_path.string()
              << " frame=" << header.frame_number
              << " size=" << header.width << 'x' << header.height
              << " bytes=" << bytes.size() << '\n';
    return 0;
}
