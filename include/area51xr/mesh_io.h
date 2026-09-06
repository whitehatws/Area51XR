#pragma once

#include "area51xr/depth_mesh.h"

#include <filesystem>

namespace area51xr {

bool write_obj_mesh(const std::filesystem::path& path, const DepthMesh& mesh);

} // namespace area51xr
