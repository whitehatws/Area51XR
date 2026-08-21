#include "area51xr/mesh_io.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    area51xr::DepthMesh mesh{};
    mesh.vertices = {
        {{-1.0f, 1.0f, -2.0f}, 0.0f, 0.0f},
        {{-1.0f, -1.0f, -2.0f}, 0.0f, 1.0f},
        {{1.0f, 1.0f, -2.0f}, 1.0f, 0.0f}
    };
    mesh.indices = {0, 1, 2};

    const auto path = std::filesystem::temp_directory_path() / "area51xr_mesh_io_test.obj";
    assert(area51xr::write_obj_mesh(path, mesh));

    std::ifstream in(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(text.find("v -1 1 -2") != std::string::npos);
    assert(text.find("vt 0 1") != std::string::npos);
    assert(text.find("f 1/1 2/2 3/3") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove(path, ec);
    return 0;
}
