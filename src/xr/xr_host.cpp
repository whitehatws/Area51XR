#include "area51xr/xr_host.h"

namespace area51xr {

XrHost::XrHost(XrRuntime& runtime, MameSharedState& shared, AimPlane plane)
    : runtime_(runtime), shared_(shared), plane_(plane), frame_buffer_(kMameFrameBufferBytes) {}

bool XrHost::initialize() {
    initialized_ = runtime_.initialize();
    return initialized_;
}

HostTickResult XrHost::tick() {
    HostTickResult result{};
    result.frame = read_frame_status(shared_);

    if (!initialized_) {
        return result;
    }

    XrInputState input{};
    result.runtime_ok = runtime_.poll(input);
    if (!result.runtime_ok) {
        return result;
    }

    result.session_running = input.session_running;
    result.trigger_down = input.trigger_down;

    if (input.session_running && input.pose_valid) {
        if (const auto projected = project_aim_to_plane(input.aim, plane_)) {
            last_aim_ = *projected;
            result.aim_valid = true;
            result.aim = *projected;
        }
    }

    if (!result.aim_valid) {
        result.aim = last_aim_;
    }

    write_gun_state(
        shared_,
        result.aim.x,
        result.aim.y,
        result.session_running && input.pose_valid && result.trigger_down);

    if (result.session_running && result.frame.frame_number != 0 &&
        result.frame.frame_number != last_presented_frame_) {
        MameFrameHeader header{};
        if (copy_latest_frame(shared_, header, frame_buffer_)) {
            VideoFrameView frame{};
            frame.frame_number = header.frame_number;
            frame.width = header.width;
            frame.height = header.height;
            frame.stride_bytes = header.stride_bytes;
            frame.pixel_format = header.pixel_format;
            frame.pixels = std::span<const std::uint8_t>(
                frame_buffer_.data(), static_cast<std::size_t>(header.payload_bytes));

            result.frame_presented = runtime_.present(frame);
            if (!result.frame_presented) {
                result.runtime_ok = false;
                return result;
            }
            last_presented_frame_ = header.frame_number;
        }
    }

    return result;
}

void XrHost::shutdown() {
    if (initialized_) {
        runtime_.shutdown();
    }
    initialized_ = false;
}

} // namespace area51xr
