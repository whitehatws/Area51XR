#include "area51xr/depth_inference.h"
#include "area51xr/depth_provider.h"
#include "area51xr/mame_ipc.h"
#include "area51xr/reconstruction_capture.h"

#ifdef A51XR_HAS_ONNXRUNTIME
#include "area51xr/onnx_depth_backend.h"
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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
    std::cerr << "usage: area51xr-capture <output.a51cap> [timeout-ms] [depth-model.onnx]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        print_usage();
        return 1;
    }

    const std::filesystem::path output_path = argv[1];
    int timeout_ms = 10000;
    if (argc >= 3) {
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
    const std::filesystem::path model_path = argc == 4 ? std::filesystem::path(argv[3]) : std::filesystem::path{};

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

    area51xr::SyntheticDepthProvider synthetic_provider(2.0f, 0.25f);
    area51xr::DepthProvider* depth_provider = &synthetic_provider;

#ifdef A51XR_HAS_ONNXRUNTIME
    std::unique_ptr<area51xr::OnnxRuntimeBackend> onnx_backend;
    std::unique_ptr<area51xr::DepthAnythingV2Provider> model_provider;
    if (!model_path.empty()) {
        onnx_backend = std::make_unique<area51xr::OnnxRuntimeBackend>(model_path);
        if (!onnx_backend->valid()) {
            std::cerr << "failed to initialize ONNX depth model: " << onnx_backend->last_error() << '\n';
            return 4;
        }
        model_provider = std::make_unique<area51xr::DepthAnythingV2Provider>(*onnx_backend);
        depth_provider = model_provider.get();
    }
#else
    if (!model_path.empty()) {
        std::cerr << "this build does not include ONNX Runtime support\n";
        return 4;
    }
#endif

    area51xr::DepthEstimate depth{};
    if (!depth_provider->estimate(frame, depth)) {
        std::cerr << "failed to estimate capture depth\n";
        return 5;
    }

    const auto bytes = area51xr::encode_reconstruction_capture(frame, depth.depth_m);
    if (bytes.empty()) {
        std::cerr << "failed to encode reconstruction capture\n";
        return 6;
    }

    if (!write_file(output_path, bytes)) {
        std::cerr << "failed to write capture: " << output_path.string() << '\n';
        return 7;
    }

    std::cout << "capture=" << output_path.string()
              << " frame=" << header.frame_number
              << " size=" << header.width << 'x' << header.height
              << " depth=" << (model_path.empty() ? "synthetic" : "model")
              << " bytes=" << bytes.size() << '\n';
    return 0;
}
