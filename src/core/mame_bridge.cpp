#include "area51xr/mame_bridge.h"

#include <algorithm>
#include <cmath>

namespace area51xr {
namespace {

std::uint16_t to_9bit(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint16_t>(std::lround(clamped * 511.0f));
}

}  // namespace

CoJagGunRegisters encode_cojag_gun(float normalized_x, float normalized_y) {
    const std::uint16_t x = to_9bit(normalized_x);
    const std::uint16_t y = to_9bit(normalized_y);

    CoJagGunRegisters result{};
    result.x = x;
    result.y = y;
    result.packed = (static_cast<std::uint32_t>(y) << 16) |
                    static_cast<std::uint32_t>(x ^ 0x01ffu);
    return result;
}

}  // namespace area51xr
