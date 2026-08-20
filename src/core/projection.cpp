#include "core/projection.hpp"

#include <algorithm>

namespace a51xr {

std::optional<Vec2> normalized_to_gun(Vec2 normalized, GunBounds bounds) {
    if (bounds.width <= 0 || bounds.height <= 0) {
        return std::nullopt;
    }

    if (normalized.x < 0.0 || normalized.x > 1.0 ||
        normalized.y < 0.0 || normalized.y > 1.0) {
        return std::nullopt;
    }

    const auto max_x = static_cast<double>(bounds.width - 1);
    const auto max_y = static_cast<double>(bounds.height - 1);

    return Vec2{
        std::clamp(normalized.x * max_x, 0.0, max_x),
        std::clamp((1.0 - normalized.y) * max_y, 0.0, max_y),
    };
}

} // namespace a51xr
