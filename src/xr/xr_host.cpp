#include "area51xr/xr_host.h"
#include "area51xr/vr_ui.h"

#include <chrono>
#include <span>

namespace area51xr {
namespace {
constexpr std::uint32_t kStartupWidth=640, kStartupHeight=360;
constexpr auto kStartupDuration=std::chrono::milliseconds(2500);
std::uint8_t next_token(std::uint8_t value) { ++value; return value ? value : 1; }
}

XrHost::XrHost(XrRuntime& runtime, MameSharedState& shared, AimPlane plane)
    : runtime_(runtime), shared_(shared), plane_(plane), frame_buffer_(kMameFrameBufferBytes),
      startup_buffer_(static_cast<std::size_t>(kStartupWidth)*kStartupHeight*4u) {}

bool XrHost::initialize() { initialized_=runtime_.initialize(); return initialized_; }

HostTickResult XrHost::tick() {
    HostTickResult result{};
    if(!initialized_) return result;

    XrInputState input{};
    result.runtime_ok=runtime_.poll(input);
    if(!result.runtime_ok) return result;
    result.session_running=input.session_running;
    result.trigger_down=input.trigger_down;
    result.coin_down=input.coin_down;
    result.start_down=input.start_down;

    const auto now=std::chrono::steady_clock::now();
    if(input.session_running && !startup_started_) {
        startup_started_=true;
        startup_started_at_=now;
    }
    result.startup_active=startup_started_ && now-startup_started_at_<kStartupDuration;

    result.frame=read_frame_status(shared_);
    if(result.frame.width && result.frame.height)
        plane_.height=plane_.width*static_cast<float>(result.frame.height)/static_cast<float>(result.frame.width);

    if(input.session_running && input.pose_valid) {
        if(const auto projected=project_aim_to_plane(input.aim,plane_)) {
            last_aim_=*projected; result.aim_valid=true; result.aim=*projected;
        } else result.offscreen=true;
    }
    if(!result.aim_valid) result.aim=last_aim_;

    const bool menu_pressed=input.menu_down && !last_menu_down_;
    const bool trigger_pressed=input.trigger_down && !last_trigger_down_;
    if(menu_pressed) menu_open_=!menu_open_;

    VrMenuItem selected=VrMenuItem::none;
    if(menu_open_ && result.aim_valid) selected=menu_item_at(result.aim.x,result.aim.y);
    if(menu_open_ && trigger_pressed) {
        if(selected==VrMenuItem::resume) menu_open_=false;
        else if(selected==VrMenuItem::restart) {
            restart_token_=next_token(restart_token_); result.restart_requested=true; menu_open_=false;
        } else if(selected==VrMenuItem::quit) {
            quit_token_=next_token(quit_token_); result.quit_requested=true;
        }
    }

    result.menu_open=menu_open_;
    const bool gameplay=!menu_open_;
    write_gun_state(shared_,result.aim.x,result.aim.y,
        gameplay && input.session_running && input.pose_valid && input.trigger_down,
        gameplay && result.offscreen,
        gameplay && input.session_running && input.coin_down,
        gameplay && input.session_running && input.start_down,
        menu_open_,restart_token_,quit_token_);

    if(result.frame.frame_number && result.frame.frame_number!=last_copied_frame_) {
        MameFrameHeader header{};
        if(copy_latest_frame(shared_,header,frame_buffer_)) {
            cached_frame_=header; last_copied_frame_=header.frame_number; has_frame_=true;
        }
    }

    if(result.session_running) {
        VideoFrameView frame{};
        if(result.startup_active) {
            render_startup_screen(startup_buffer_,kStartupWidth,kStartupHeight,kStartupWidth*4u);
            frame.frame_number=input.sample_number;
            frame.width=kStartupWidth; frame.height=kStartupHeight;
            frame.stride_bytes=kStartupWidth*4u; frame.pixel_format=1;
            frame.pixels=std::span<const std::uint8_t>(startup_buffer_);
        } else if(menu_open_) {
            std::uint32_t w=kStartupWidth,h=kStartupHeight,s=kStartupWidth*4u;
            std::size_t bytes=startup_buffer_.size();
            if(has_frame_) {
                w=cached_frame_.width; h=cached_frame_.height; s=cached_frame_.stride_bytes;
                bytes=static_cast<std::size_t>(cached_frame_.payload_bytes);
                ui_buffer_.assign(frame_buffer_.begin(),frame_buffer_.begin()+bytes);
            } else ui_buffer_.assign(bytes,0);
            render_pause_menu(ui_buffer_,w,h,s,selected,result.aim.x,result.aim.y,result.aim_valid);
            frame.frame_number=input.sample_number;
            frame.width=w; frame.height=h; frame.stride_bytes=s; frame.pixel_format=1;
            frame.pixels=std::span<const std::uint8_t>(ui_buffer_);
        } else if(has_frame_) {
            frame.frame_number=cached_frame_.frame_number;
            frame.width=cached_frame_.width; frame.height=cached_frame_.height;
            frame.stride_bytes=cached_frame_.stride_bytes; frame.pixel_format=cached_frame_.pixel_format;
            frame.pixels=std::span<const std::uint8_t>(frame_buffer_.data(),
                static_cast<std::size_t>(cached_frame_.payload_bytes));
        }
        result.frame_presented=runtime_.present(frame);
        if(!result.frame_presented) { result.runtime_ok=false; return result; }
    }

    last_menu_down_=input.menu_down;
    last_trigger_down_=input.trigger_down;
    return result;
}

void XrHost::shutdown() {
    if(initialized_) runtime_.shutdown();
    initialized_=false;
}

} // namespace area51xr
