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

    if (result.frame.frame_number != 0 && result.frame.frame_number != last_copied_frame_) {
        MameFrameHeader header{};
        if (copy_latest_frame(shared_, header, frame_buffer_)) {
            cached_frame_ = header;
            last_copied_frame_ = header.frame_number;
            has_frame_ = true;
            if (header.width != 0 && header.height != 0) {
                plane_.height = plane_.width *
                    static_cast<float>(header.height) / static_cast<float>(header.width);
            }
        }
    }

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

    if (result.session_running) {
        VideoFrameView frame{};
        if (has_frame_) {
            frame.frame_number = cached_frame_.frame_number;
            frame.width = cached_frame_.width;
            frame.height = cached_frame_.height;
            frame.stride_bytes = cached_frame_.stride_bytes;
            frame.pixel_format = cached_frame_.pixel_format;
            frame.pixels = std::span<const std::uint8_t>(
                frame_buffer_.data(), static_cast<std::size_t>(cached_frame_.payload_bytes));
        }

        result.frame_presented = runtime_.present(frame);
        if (!result.frame_presented) {
            result.runtime_ok = false;
            return result;
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
