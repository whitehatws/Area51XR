#pragma once

#include <cstdint>
#include <span>

namespace area51xr {

enum class VrMenuItem : std::int8_t { none = -1, resume = 0, restart = 1, quit = 2 };

VrMenuItem menu_item_at(float normalized_x, float normalized_y) noexcept;

void render_startup_screen(
    std::span<std::uint8_t> pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t stride_bytes);

void render_pause_menu(
    std::span<std::uint8_t> pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t stride_bytes,
    VrMenuItem selected,
    float pointer_x,
    float pointer_y,
    bool pointer_valid);

} // namespace area51xr
