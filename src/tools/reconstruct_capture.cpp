#include "area51xr/depth_mesh.h"
#include "area51xr/mesh_io.h"
#include "area51xr/reconstruction_capture.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: area51xr-reconstruct <capture.a51cap> <mesh.obj>\n";
        return 1;
    }

    const std::filesystem::path input_path = argv[1];
    const std::filesystem::path output_path = argv[2];
    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        std::cerr << "failed to open capture: " << input_path.string() << '\n';
        return 2;
    }

    const std::vector<std::uint8_t> bytes(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
    area51xr::ReconstructionCapture capture{};
    if (!area51xr::decode_reconstruction_capture(bytes, capture)) {
        std::cerr << "invalid or corrupted reconstruction capture\n";
        return 3;
    }

    constexpr float kDefaultVerticalFovRadians = 1.04719755f;
    const auto camera = area51xr::intrinsics_from_vertical_fov(
        capture.width, capture.height, kDefaultVerticalFovRadians);
    area51xr::DepthMeshOptions options{};
    options.sample_step = 2;
    const auto mesh = area51xr::build_depth_mesh(
        capture.depth_m, capture.width, capture.height, camera, options);
    if (mesh.indices.empty()) {
        std::cerr << "capture produced no valid reconstruction triangles\n";
        return 4;
    }
    if (!area51xr::write_obj_mesh(output_path, mesh)) {
        std::cerr << "failed to write OBJ: " << output_path.string() << '\n';
        return 5;
    }

    std::cout << "frame=" << capture.frame_number
              << " size=" << capture.width << 'x' << capture.height
              << " vertices=" << mesh.vertices.size()
              << " triangles=" << mesh.indices.size() / 3
              << " obj=" << output_path.string() << '\n';
    return 0;
}
