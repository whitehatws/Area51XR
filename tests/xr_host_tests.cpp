#include "area51xr/xr_host.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <memory>

namespace {
bool near(float a, float b) { return std::abs(a - b) < 1.0e-4f; }
}

int main() {
    auto shared = std::make_unique<area51xr::MameSharedState>();
    shared->protocol_version = area51xr::kMameBridgeProtocolVersion;

    area51xr::MameFrameHeader frame{};
    frame.frame_number = 12;
    frame.width = 1;
    frame.height = 1;
    frame.stride_bytes = 4;
    frame.pixel_format = 1;
    const std::array<std::uint8_t, 4> pixel{10, 20, 30, 255};
    frame.payload_bytes = pixel.size();
    assert(area51xr::publish_frame(*shared, frame, pixel));

    area51xr::XrInputState input{};
    input.session_running = true;
    input.pose_valid = true;
    input.aim.position = {0.0f, 0.0f, 0.0f};
    input.trigger_down = true;
    input.coin_down = true;
    input.start_down = true;

    area51xr::SimulatedXrRuntime runtime(input);
    area51xr::XrHost host(runtime, *shared, {}, false);
    assert(host.initialize());

    const auto first = host.tick();
    assert(first.runtime_ok);
    assert(first.session_running);
    assert(first.aim_valid);
    assert(!first.offscreen);
    assert(near(first.aim.x, 0.5f));
    assert(near(first.aim.y, 0.5f));
    assert(first.trigger_down);
    assert(first.coin_down);
    assert(first.start_down);
    assert(!first.reticle_enabled);
    assert(first.passthrough == area51xr::PassthroughState::unavailable);
    assert(first.frame.frame_number == 12);
    assert(first.frame_presented);
    assert(runtime.last_presented_frame() == 12);
    assert(shared->gun.trigger == 1);
    assert(shared->gun.offscreen == 0);
    assert(shared->gun.coin == 1);
    assert(shared->gun.start == 1);
    assert(near(shared->gun.aim_x, 0.5f));
    assert(near(shared->gun.aim_y, 0.5f));

    // Reticle is default-off, toggles immediately, and renders at the active
    // controller's projected hit without changing the coordinates sent to MAME.
    std::vector<std::uint8_t> larger_pixels(100u * 100u * 4u, 0);
    frame.frame_number = 13;
    frame.width = 100;
    frame.height = 100;
    frame.stride_bytes = 400;
    frame.payload_bytes = larger_pixels.size();
    assert(area51xr::publish_frame(*shared, frame, larger_pixels));

    input.trigger_down = false;
    input.menu_down = true;
    input.aim.position = {0.0f, 0.14f, 0.0f};
    runtime.set_state(input);
    assert(host.tick().menu_open);
    input.menu_down = false;
    runtime.set_state(input);
    host.tick();
    input.trigger_down = true;
    input.active_hand = area51xr::ControllerHand::left;
    runtime.set_state(input);
    const auto toggled = host.tick();
    assert(toggled.menu_open);
    assert(toggled.settings_changed);
    assert(toggled.reticle_enabled);
    assert(toggled.active_hand == area51xr::ControllerHand::left);
    assert(near(shared->gun.aim_y, 0.40f));

    input.trigger_down = false;
    runtime.set_state(input);
    host.tick();
    input.menu_down = true;
    runtime.set_state(input);
    const auto gameplay = host.tick();
    assert(!gameplay.menu_open);
    assert(gameplay.reticle_enabled);
    assert(runtime.last_presented_pixels().size() == larger_pixels.size());
    assert(std::any_of(runtime.last_presented_pixels().begin(),
        runtime.last_presented_pixels().end(),[](auto value){return value==225;}));

    input.menu_down = false;
    input.active_hand = area51xr::ControllerHand::right;
    input.aim.position = {0.14f, 0.0f, 0.0f};
    runtime.set_state(input);
    const auto switched = host.tick();
    assert(switched.active_hand == area51xr::ControllerHand::right);
    assert(near(switched.aim.x, 0.60f));
    assert(near(shared->gun.aim_x, 0.60f));

    input.menu_down = true;
    input.trigger_down = false;
    input.coin_down = false;
    input.start_down = false;
    runtime.set_state(input);
    const auto menu = host.tick();
    assert(menu.menu_open);
    assert(shared->gun.pause == 1);
    assert(shared->gun.trigger == 0);

    input.menu_down = false;
    runtime.set_state(input);
    host.tick();
    input.menu_down = true;
    runtime.set_state(input);
    const auto resumed = host.tick();
    assert(!resumed.menu_open);
    assert(shared->gun.pause == 0);
    input.menu_down = false;
    runtime.set_state(input);

    // A tracked ray that misses the game plane is a real off-screen gun state.
    // Pulling the trigger here is how Area 51 performs its cabinet reload.
    input.aim.position = {2.0f, 0.0f, 0.0f};
    input.pose_valid = true;
    input.trigger_down = true;
    input.coin_down = false;
    input.start_down = false;
    runtime.set_state(input);
    const auto offscreen = host.tick();
    assert(!offscreen.aim_valid);
    assert(offscreen.offscreen);
    assert(offscreen.trigger_down);
    assert(!offscreen.coin_down);
    assert(!offscreen.start_down);
    assert(shared->gun.trigger == 1);
    assert(shared->gun.offscreen == 1);
    assert(shared->gun.coin == 0);
    assert(shared->gun.start == 0);
    assert(near(shared->gun.aim_x, 0.60f));
    assert(near(shared->gun.aim_y, 0.5f));

    input.pose_valid = false;
    input.trigger_down = true;
    runtime.set_state(input);
    const auto invalid = host.tick();
    assert(!invalid.aim_valid);
    assert(!invalid.offscreen);
    assert(invalid.frame_presented);
    assert(runtime.last_presented_frame() == 13);
    assert(shared->gun.trigger == 0);
    assert(shared->gun.offscreen == 0);
    assert(near(shared->gun.aim_x, 0.60f));
    assert(near(shared->gun.aim_y, 0.5f));

    host.shutdown();

    auto passthrough_shared = std::make_unique<area51xr::MameSharedState>();
    passthrough_shared->protocol_version = area51xr::kMameBridgeProtocolVersion;
    area51xr::XrInputState passthrough_input{};
    passthrough_input.session_running = true;
    passthrough_input.pose_valid = true;
    passthrough_input.aim.position = {0.0f, -0.021f, 0.0f};
    area51xr::SimulatedXrRuntime passthrough_runtime(passthrough_input);
    passthrough_runtime.set_passthrough_supported(true);
    area51xr::XrHost passthrough_host(
        passthrough_runtime,*passthrough_shared,{},false);
    assert(passthrough_host.initialize());
    assert(!passthrough_host.settings().passthrough_enabled);

    passthrough_input.menu_down = true;
    passthrough_runtime.set_state(passthrough_input);
    assert(passthrough_host.tick().menu_open);
    passthrough_input.menu_down = false;
    passthrough_runtime.set_state(passthrough_input);
    passthrough_host.tick();
    passthrough_input.trigger_down = true;
    passthrough_runtime.set_state(passthrough_input);
    const auto passthrough_on = passthrough_host.tick();
    assert(passthrough_on.settings_changed);
    assert(passthrough_on.passthrough == area51xr::PassthroughState::on);
    assert(passthrough_host.settings().passthrough_enabled);

    const auto held = passthrough_host.tick();
    assert(!held.settings_changed);
    assert(held.passthrough == area51xr::PassthroughState::on);

    passthrough_input.session_running = false;
    passthrough_runtime.set_state(passthrough_input);
    const auto session_lost = passthrough_host.tick();
    assert(session_lost.runtime_ok);
    assert(!session_lost.session_running);
    passthrough_host.shutdown();

    auto action_shared = std::make_unique<area51xr::MameSharedState>();
    action_shared->protocol_version = area51xr::kMameBridgeProtocolVersion;
    area51xr::XrInputState action_input{};
    action_input.session_running = true;
    action_input.pose_valid = true;
    action_input.aim.position = {0.0f, -0.147f, 0.0f};
    area51xr::SimulatedXrRuntime action_runtime(action_input);
    area51xr::XrHost action_host(action_runtime,*action_shared,{},false);
    assert(action_host.initialize());
    action_input.menu_down = true;
    action_runtime.set_state(action_input);
    assert(action_host.tick().menu_open);
    action_input.menu_down = false;
    action_runtime.set_state(action_input);
    action_host.tick();
    action_input.trigger_down = true;
    action_runtime.set_state(action_input);
    const auto restarted = action_host.tick();
    assert(restarted.restart_requested);
    assert(!restarted.menu_open);
    assert(action_shared->gun.restart_token != 0);
    const auto restart_token = action_shared->gun.restart_token;
    const auto held_restart = action_host.tick();
    assert(!held_restart.restart_requested);
    assert(action_shared->gun.restart_token == restart_token);
    action_host.shutdown();

    auto quit_shared = std::make_unique<area51xr::MameSharedState>();
    quit_shared->protocol_version = area51xr::kMameBridgeProtocolVersion;
    area51xr::XrInputState quit_input{};
    quit_input.session_running = true;
    quit_input.pose_valid = true;
    quit_input.aim.position = {0.0f, -0.273f, 0.0f};
    area51xr::SimulatedXrRuntime quit_runtime(quit_input);
    area51xr::XrHost quit_host(quit_runtime,*quit_shared,{},false);
    assert(quit_host.initialize());
    quit_input.menu_down = true;
    quit_runtime.set_state(quit_input);
    assert(quit_host.tick().menu_open);
    quit_input.menu_down = false;
    quit_runtime.set_state(quit_input);
    quit_host.tick();
    quit_input.trigger_down = true;
    quit_runtime.set_state(quit_input);
    const auto quit = quit_host.tick();
    assert(quit.quit_requested);
    assert(quit_shared->gun.quit_token != 0);
    quit_host.shutdown();
    return 0;
}
