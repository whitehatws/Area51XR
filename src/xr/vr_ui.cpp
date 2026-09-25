#include "area51xr/vr_ui.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace area51xr {
namespace {

struct Color { std::uint8_t b, g, r, a; };
using Glyph = std::array<std::uint8_t, 7>;

Glyph glyph(char c) noexcept {
    switch (c) {
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '5': return {31,16,30,1,1,17,14};
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case ':': return {0,4,4,0,4,4,0};
    default: return {};
    }
}

bool valid(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h, std::uint32_t s) {
    return w && h && s >= w * 4u && p.size() >= static_cast<std::size_t>(s) * h;
}

void pixel(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h,
    std::uint32_t s, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h)) return;
    const std::size_t o = static_cast<std::size_t>(y) * s + static_cast<std::size_t>(x) * 4u;
    p[o]=c.b; p[o+1]=c.g; p[o+2]=c.r; p[o+3]=c.a;
}

void rect(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h,
    std::uint32_t s, int l, int t, int r, int b, Color c) {
    l=std::max(l,0); t=std::max(t,0); r=std::min(r,static_cast<int>(w)); b=std::min(b,static_cast<int>(h));
    for(int y=t;y<b;++y) for(int x=l;x<r;++x) pixel(p,w,h,s,x,y,c);
}

int text_width(std::string_view text, int scale) {
    return text.empty() ? 0 : (static_cast<int>(text.size()) * 6 - 1) * scale;
}

void text(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h,
    std::uint32_t s, int x, int y, int scale, std::string_view value, Color c) {
    for(char ch:value) {
        const auto rows=glyph(ch);
        for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(rows[static_cast<std::size_t>(row)] & (1u << (4-col)))
                rect(p,w,h,s,x+col*scale,y+row*scale,x+(col+1)*scale,y+(row+1)*scale,c);
        x += 6*scale;
    }
}

void centered(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h,
    std::uint32_t s, int y, int scale, std::string_view value, Color c) {
    text(p,w,h,s,(static_cast<int>(w)-text_width(value,scale))/2,y,scale,value,c);
}

void border(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h,
    std::uint32_t s, int l, int t, int r, int b, int n, Color c) {
    rect(p,w,h,s,l,t,r,t+n,c); rect(p,w,h,s,l,b-n,r,b,c);
    rect(p,w,h,s,l,t,l+n,b,c); rect(p,w,h,s,r-n,t,r,b,c);
}

} // namespace

VrMenuItem menu_item_at(float x, float y) noexcept {
    if(x<0.20f || x>0.80f) return VrMenuItem::none;
    constexpr float top=0.24f, height=0.10f, gap=0.02f;
    for(int i=0;i<5;++i) {
        const float y0=top+static_cast<float>(i)*(height+gap);
        if(y>=y0 && y<=y0+height) return static_cast<VrMenuItem>(i);
    }
    return VrMenuItem::none;
}

void render_startup_screen(std::span<std::uint8_t> p, std::uint32_t w,
    std::uint32_t h, std::uint32_t s) {
    if(!valid(p,w,h,s)) return;
    for(std::uint32_t y=0;y<h;++y) {
        const auto red=static_cast<std::uint8_t>(8u+(28u*y)/h);
        rect(p,w,h,s,0,static_cast<int>(y),static_cast<int>(w),static_cast<int>(y+1),{4,3,red,255});
    }
    const Color white{245,245,245,255}, red{34,42,220,255}, muted{150,155,170,255};
    const int title=std::max(2,static_cast<int>(w/92u));
    const int game=std::max(2,static_cast<int>(w/128u));
    centered(p,w,h,s,static_cast<int>(h*28u/100u),title,"FORTYDUBZ",white);
    rect(p,w,h,s,static_cast<int>(w*28u/100u),static_cast<int>(h*50u/100u),
        static_cast<int>(w*72u/100u),static_cast<int>(h*50u/100u)+3,red);
    centered(p,w,h,s,static_cast<int>(h*57u/100u),std::max(2,game-1),"PRESENTS",muted);
    centered(p,w,h,s,static_cast<int>(h*73u/100u),game,"AREA51XR",red);
}

void render_pause_menu(std::span<std::uint8_t> p, std::uint32_t w, std::uint32_t h,
    std::uint32_t s, VrMenuItem selected, bool reticle_enabled,
    PassthroughState passthrough, float px, float py, bool pointer_valid) {
    if(!valid(p,w,h,s)) return;
    for(std::uint32_t y=0;y<h;++y) for(std::uint32_t x=0;x<w;++x) {
        const std::size_t o=static_cast<std::size_t>(y)*s+static_cast<std::size_t>(x)*4u;
        p[o]/=4u; p[o+1]/=4u; p[o+2]/=4u; p[o+3]=255;
    }
    const Color panel{13,13,18,255}, white{245,245,245,255}, red{32,42,225,255};
    const Color selected_fill{24,30,92,255}, muted{100,105,120,255};
    const int scale=std::max(1,static_cast<int>(w/160u));
    const int l=static_cast<int>(w*16u/100u), r=static_cast<int>(w*84u/100u);
    const int t=static_cast<int>(h*8u/100u), b=static_cast<int>(h*94u/100u);
    rect(p,w,h,s,l,t,r,b,panel); border(p,w,h,s,l,t,r,b,std::max(1,scale),red);
    centered(p,w,h,s,static_cast<int>(h*14u/100u),scale+1,"PAUSED",white);

    const std::array<std::string_view,5> labels{
        "RESUME",
        reticle_enabled ? "RETICLE: ON" : "RETICLE: OFF",
        passthrough == PassthroughState::unavailable ? "PASSTHROUGH: UNAVAILABLE" :
            (passthrough == PassthroughState::on ? "PASSTHROUGH: ON" : "PASSTHROUGH: OFF"),
        "RESTART GAME",
        "QUIT AREA51XR"};
    constexpr float top=0.24f, bh=0.10f, gap=0.02f;
    for(int i=0;i<5;++i) {
        const int y0=static_cast<int>(h*(top+static_cast<float>(i)*(bh+gap)));
        const int y1=static_cast<int>(h*(top+static_cast<float>(i)*(bh+gap)+bh));
        const bool active=selected==static_cast<VrMenuItem>(i);
        if(active) rect(p,w,h,s,l+scale*3,y0,r-scale*3,y1,selected_fill);
        border(p,w,h,s,l+scale*3,y0,r-scale*3,y1,std::max(1,scale),active?red:muted);
        const auto label=labels[static_cast<std::size_t>(i)];
        const int available=std::max(1,r-l-scale*10);
        const int label_scale=std::max(1,std::min(scale,
            available/std::max(1,static_cast<int>(label.size())*6-1)));
        centered(p,w,h,s,y0+(y1-y0-7*label_scale)/2,label_scale,label,white);
    }
    centered(p,w,h,s,static_cast<int>(h*86u/100u),std::max(1,scale-1),
        "POINT AND PULL TRIGGER",muted);
    if(pointer_valid) {
        const int x=static_cast<int>(std::clamp(px,0.0f,1.0f)*static_cast<float>(w-1u));
        const int y=static_cast<int>(std::clamp(py,0.0f,1.0f)*static_cast<float>(h-1u));
        const int q=std::max(3,scale*3);
        rect(p,w,h,s,x-q,y-1,x+q+1,y+2,red); rect(p,w,h,s,x-1,y-q,x+2,y+q+1,red);
    }
}

void render_gameplay_reticle(std::span<std::uint8_t> p, std::uint32_t w,
    std::uint32_t h, std::uint32_t s, float px, float py) {
    if(!valid(p,w,h,s)) return;
    const Color red{32,42,225,255};
    const int scale=std::max(1,static_cast<int>(w/160u));
    const int x=static_cast<int>(std::clamp(px,0.0f,1.0f)*static_cast<float>(w-1u));
    const int y=static_cast<int>(std::clamp(py,0.0f,1.0f)*static_cast<float>(h-1u));
    const int q=std::max(3,scale*3);
    rect(p,w,h,s,x-q,y-1,x+q+1,y+2,red);
    rect(p,w,h,s,x-1,y-q,x+2,y+q+1,red);
}

} // namespace area51xr
