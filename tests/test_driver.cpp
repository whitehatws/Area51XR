#include <iostream>

int area51xr_test_projection();
int area51xr_test_mame_bridge();
int area51xr_test_mame_adapter();
int area51xr_test_bridge_host();
int area51xr_test_xr_runtime();
int area51xr_test_xr_aim();
int area51xr_test_xr_host();
int area51xr_test_vr_ui();
int area51xr_test_d3d11_mesh_renderer();
int area51xr_test_reprojection();
int area51xr_test_depth_stabilizer();
int area51xr_test_depth_reconstruction();
int area51xr_test_reconstruction_engine();
int area51xr_test_reconstruction_capture();
int area51xr_test_relative_depth();
int area51xr_test_depth_anything_v2();
int area51xr_test_depth_inference();
int area51xr_test_mesh_io();
int area51xr_test_reconstruction_quality();
int area51xr_test_reconstruction_presenter();
int area51xr_test_spatial_reconstruction_host();

namespace {

using TestFn = int (*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

} // namespace

int main() {
    const TestCase tests[] = {
        {"projection_ipc_depth_mesh", area51xr_test_projection},
        {"mame_bridge", area51xr_test_mame_bridge},
        {"mame_adapter", area51xr_test_mame_adapter},
        {"bridge_host", area51xr_test_bridge_host},
        {"xr_runtime", area51xr_test_xr_runtime},
        {"xr_aim", area51xr_test_xr_aim},
        {"xr_host", area51xr_test_xr_host},
        {"vr_ui", area51xr_test_vr_ui},
        {"d3d11_mesh_renderer", area51xr_test_d3d11_mesh_renderer},
        {"reprojection", area51xr_test_reprojection},
        {"depth_stabilizer", area51xr_test_depth_stabilizer},
        {"depth_reconstruction", area51xr_test_depth_reconstruction},
        {"reconstruction_engine", area51xr_test_reconstruction_engine},
        {"reconstruction_capture", area51xr_test_reconstruction_capture},
        {"relative_depth", area51xr_test_relative_depth},
        {"depth_anything_v2", area51xr_test_depth_anything_v2},
        {"depth_inference", area51xr_test_depth_inference},
        {"mesh_io", area51xr_test_mesh_io},
        {"reconstruction_quality", area51xr_test_reconstruction_quality},
        {"reconstruction_presenter", area51xr_test_reconstruction_presenter},
        {"spatial_reconstruction_host", area51xr_test_spatial_reconstruction_host},
    };

    for (const auto& test : tests) {
        std::cout << "[ RUN      ] " << test.name << std::endl;
        const int result = test.fn();
        if (result != 0) {
            std::cerr << "[  FAILED  ] " << test.name << " (exit " << result << ")\n";
            return result;
        }
        std::cout << "[       OK ] " << test.name << std::endl;
    }

    std::cout << "All Area51XR regressions passed.\n";
    return 0;
}
