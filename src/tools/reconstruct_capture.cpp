#include "area51xr/depth_mesh.h"
#include "area51xr/depth_reconstruction.h"
#include "area51xr/mesh_io.h"
#include "area51xr/reconstruction_capture.h"
#include "area51xr/reconstruction_quality.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

bool write_quality_report(
    const std::filesystem::path& path,
    const area51xr::ReconstructionCapture& capture,
    const area51xr::DepthReconstructionStats& stats,
    const area51xr::ReconstructionQualityResult& quality,
    const std::filesystem::path& obj_path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    out << std::fixed << std::setprecision(6);
    out << "{\n";
    out << "  \"passed\": " << (quality.passed ? "true" : "false") << ",\n";
    out << "  \"reason\": \"" << json_escape(quality.reason) << "\",\n";
    out << "  \"capture\": {\n";
    out << "    \"frame_number\": " << capture.frame_number << ",\n";
    out << "    \"width\": " << capture.width << ",\n";
    out << "    \"height\": " << capture.height << "\n";
    out << "  },\n";
    out << "  \"reconstruction\": {\n";
    out << "    \"frame_number\": " << stats.frame_number << ",\n";
    out << "    \"total_samples\": " << stats.total_samples << ",\n";
    out << "    \"confident_samples\": " << stats.confident_samples << ",\n";
    out << "    \"valid_depth_samples\": " << stats.valid_depth_samples << ",\n";
    out << "    \"mesh_triangles\": " << stats.mesh_triangles << ",\n";
    out << "    \"mesh_max_triangles\": " << stats.mesh_max_triangles << ",\n";
    out << "    \"rejected_triangles\": " << stats.rejected_triangles << ",\n";
    out << "    \"confident_sample_ratio\": " << stats.confident_sample_ratio << ",\n";
    out << "    \"valid_depth_ratio\": " << stats.valid_depth_ratio << ",\n";
    out << "    \"mesh_triangle_ratio\": " << stats.mesh_triangle_ratio << ",\n";
    out << "    \"scene_cut_reset\": " << (stats.scene_cut_reset ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"obj\": \"" << json_escape(obj_path.string()) << "\"\n";
    out << "}\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: area51xr-reconstruct <capture.a51cap> <mesh.obj> [quality.json]\n";
        return 1;
    }

    const std::filesystem::path input_path = argv[1];
    const std::filesystem::path output_path = argv[2];
    const std::filesystem::path report_path = argc == 4
        ? std::filesystem::path{argv[3]}
        : output_path.replace_extension(".quality.json");

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

    area51xr::DepthReconstructionOptions options{};
    options.stabilizer.smoothing_alpha = 1.0f;
    options.stabilizer.confidence_rise = 1.0f;
    options.mesh.sample_step = 2;

    area51xr::DepthReconstructor reconstructor(options);
    const auto& reconstruction = reconstructor.update(
        capture.depth_m, capture.width, capture.height, camera);
    const auto quality = area51xr::evaluate_reconstruction_quality(reconstruction.stats);

    if (reconstruction.mesh.indices.empty()) {
        std::cerr << "capture produced no valid reconstruction triangles\n";
        return 4;
    }
    if (!area51xr::write_obj_mesh(output_path, reconstruction.mesh)) {
        std::cerr << "failed to write OBJ: " << output_path.string() << '\n';
        return 5;
    }
    if (!write_quality_report(report_path, capture, reconstruction.stats, quality, output_path)) {
        std::cerr << "failed to write quality report: " << report_path.string() << '\n';
        return 6;
    }

    std::cout << "frame=" << capture.frame_number
              << " size=" << capture.width << 'x' << capture.height
              << " vertices=" << reconstruction.mesh.vertices.size()
              << " triangles=" << reconstruction.stats.mesh_triangles
              << " rejected=" << reconstruction.stats.rejected_triangles
              << " valid_depth=" << reconstruction.stats.valid_depth_ratio
              << " mesh_coverage=" << reconstruction.stats.mesh_triangle_ratio
              << " quality=" << (quality.passed ? "pass" : "fail")
              << " report=" << report_path.string()
              << " obj=" << output_path.string() << '\n';

    return quality.passed ? 0 : 7;
}
