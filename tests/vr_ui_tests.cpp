#include "area51xr/vr_ui.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using area51xr::VrMenuItem;
    assert(area51xr::menu_item_at(0.5f,0.36f)==VrMenuItem::resume);
    assert(area51xr::menu_item_at(0.5f,0.52f)==VrMenuItem::restart);
    assert(area51xr::menu_item_at(0.5f,0.68f)==VrMenuItem::quit);
    assert(area51xr::menu_item_at(0.1f,0.36f)==VrMenuItem::none);
    constexpr std::uint32_t w=320,h=240,s=w*4u;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(s)*h,200);
    area51xr::render_startup_screen(pixels,w,h,s);
    assert(std::any_of(pixels.begin(),pixels.end(),[](auto v){return v!=200;}));
    area51xr::render_pause_menu(pixels,w,h,s,VrMenuItem::restart,0.5f,0.52f,true);
    assert(std::any_of(pixels.begin(),pixels.end(),[](auto v){return v==225;}));
    return 0;
}
