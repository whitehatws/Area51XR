#include "area51xr/mesh_io.h"

#include <fstream>

namespace area51xr {

bool write_obj_mesh(const std::filesystem::path& path, const DepthMesh& mesh) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "# Area51XR depth mesh\n";
    for (const auto& vertex : mesh.vertices) {
        out << "v " << vertex.position.x << ' '
            << vertex.position.y << ' '
            << vertex.position.z << '\n';
    }
    for (const auto& vertex : mesh.vertices) {
        out << "vt " << vertex.u << ' ' << (1.0f - vertex.v) << '\n';
    }
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t a = mesh.indices[i] + 1;
        const std::uint32_t b = mesh.indices[i + 1] + 1;
        const std::uint32_t c = mesh.indices[i + 2] + 1;
        out << "f " << a << '/' << a << ' '
            << b << '/' << b << ' '
            << c << '/' << c << '\n';
    }
    return static_cast<bool>(out);
}

} // namespace area51xr
