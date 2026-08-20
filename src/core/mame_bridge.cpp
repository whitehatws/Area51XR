#include "area51xr/mame_bridge.h"

#include <algorithm>
#include <cmath>

namespace area51xr {
namespace {

std::uint32_t to_mame_axis(float value, std::uint32_t extent, std::uint32_t origin) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    const auto raw = static_cast<std::uint32_t>(std::lround(clamped * 255.0f));
    return origin + ((raw * extent) >> 8);
}

}  // namespace

CoJagGunRegisters encode_cojag_gun(
    float normalized_x,
    float normalized_y,
    std::uint32_t visible_width,
    std::uint32_t visible_height,
    std::uint32_t visible_left,
    std::uint32_t visible_top) {
    const auto x = static_cast<std::uint16_t>(to_mame_axis(normalized_x, visible_width, visible_left));
    const auto y = static_cast<std::uint16_t>(to_mame_axis(normalized_y, visible_height, visible_top));

    CoJagGunRegisters result{};
    result.x = x;
    result.y = y;
    result.packed = (static_cast<std::uint32_t>(y) << 16) |
                    static_cast<std::uint32_t>(x ^ 0x01ffu);
    return result;
}

}  // namespace area51xr
