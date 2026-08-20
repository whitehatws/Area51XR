#include "area51xr/mame_bridge.h"

#include <cassert>

int main() {
    using area51xr::encode_cojag_gun;

    const auto top_left = encode_cojag_gun(0.0f, 0.0f);
    assert(top_left.x == 0);
    assert(top_left.y == 0);
    assert(top_left.packed == 0x000001ffu);

    const auto bottom_right = encode_cojag_gun(1.0f, 1.0f);
    assert(bottom_right.x == 318);
    assert(bottom_right.y == 239);
    assert(bottom_right.packed == 0x00ef00c1u);

    const auto center = encode_cojag_gun(0.5f, 0.5f);
    assert(center.x == 160);
    assert(center.y == 120);
    assert(center.packed == 0x0078015fu);

    const auto clamped = encode_cojag_gun(-1.0f, 2.0f);
    assert(clamped.x == 0);
    assert(clamped.y == 239);
    assert(clamped.packed == 0x00ef01ffu);

    const auto offset_area = encode_cojag_gun(0.5f, 0.5f, 320, 240, 7, 3);
    assert(offset_area.x == 167);
    assert(offset_area.y == 123);
    assert(offset_area.packed == 0x007b0158u);

    return 0;
}
