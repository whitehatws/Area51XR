#pragma once

#include <optional>

namespace a51xr {

struct Vec2 {
    double x{};
    double y{};
};

struct GunBounds {
    int width{640};
    int height{480};
};

// Converts normalized OpenXR-style aim coordinates into the game's 2D gun space.
// Input coordinates are expected in [0, 1], with +Y pointing up.
std::optional<Vec2> normalized_to_gun(Vec2 normalized, GunBounds bounds);

} // namespace a51xr
