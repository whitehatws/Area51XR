#pragma once

#include <cstdint>
#include <span>

#include "area51xr/xr_runtime.h"

namespace area51xr {

enum class VrMenuItem : std::int8_t {
    none = -1,
    resume = 0,
    reticle = 1,
    passthrough = 2,
    restart = 3,
    quit = 4
};

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
    bool reticle_enabled,
    PassthroughState passthrough,
    float pointer_x,
    float pointer_y,
    bool pointer_valid);

void render_gameplay_reticle(
    std::span<std::uint8_t> pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t stride_bytes,
    float normalized_x,
    float normalized_y);

} // namespace area51xr
