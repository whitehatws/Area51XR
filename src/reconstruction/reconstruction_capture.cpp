#include "area51xr/reconstruction_capture.h"

#include <array>
#include <bit>
#include <cstring>
#include <limits>

namespace area51xr {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'A','5','1','X','R','C','A','P'};

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

std::uint32_t checksum(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t hash = 2166136261u;
    for (const std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

bool read_u32(std::span<const std::uint8_t> bytes, std::size_t& offset, std::uint32_t& out) noexcept {
    if (offset + 4 > bytes.size()) return false;
    out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        out |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    }
    return true;
}

bool read_u64(std::span<const std::uint8_t> bytes, std::size_t& offset, std::uint64_t& out) noexcept {
    if (offset + 8 > bytes.size()) return false;
    out = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        out |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    }
    return true;
}

} // namespace

std::vector<std::uint8_t> encode_reconstruction_capture(
    const ColorFrameView& frame,
    std::span<const float> depth_m) {
    const std::size_t pixel_bytes = static_cast<std::size_t>(frame.stride_bytes) * frame.height;
    const std::size_t depth_count = static_cast<std::size_t>(frame.width) * frame.height;
    if (frame.width == 0 || frame.height == 0 || frame.stride_bytes < frame.width * 4u ||
        frame.pixels.size() < pixel_bytes || depth_m.size() != depth_count) {
        return {};
    }

    std::vector<std::uint8_t> out;
    const std::size_t header_bytes = 8 + 4 + 8 + 4 + 4 + 4 + 8 + 8;
    const std::size_t depth_bytes = depth_count * sizeof(float);
    out.reserve(header_bytes + pixel_bytes + depth_bytes + 4);
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    append_u32(out, kReconstructionCaptureVersion);
    append_u64(out, frame.frame_number);
    append_u32(out, frame.width);
    append_u32(out, frame.height);
    append_u32(out, frame.stride_bytes);
    append_u64(out, static_cast<std::uint64_t>(pixel_bytes));
    append_u64(out, static_cast<std::uint64_t>(depth_count));
    out.insert(out.end(), frame.pixels.begin(), frame.pixels.begin() + static_cast<std::ptrdiff_t>(pixel_bytes));
    for (const float value : depth_m) {
        append_u32(out, std::bit_cast<std::uint32_t>(value));
    }
    append_u32(out, checksum(out));
    return out;
}

bool decode_reconstruction_capture(
    std::span<const std::uint8_t> bytes,
    ReconstructionCapture& output) {
    output = {};
    if (bytes.size() < 52 || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        return false;
    }

    const std::size_t checksum_offset = bytes.size() - 4;
    std::size_t checksum_read = checksum_offset;
    std::uint32_t stored_checksum{};
    if (!read_u32(bytes, checksum_read, stored_checksum) ||
        stored_checksum != checksum(bytes.first(checksum_offset))) {
        return false;
    }

    std::size_t offset = kMagic.size();
    std::uint32_t version{};
    std::uint64_t pixel_bytes{};
    std::uint64_t depth_count{};
    if (!read_u32(bytes, offset, version) || version != kReconstructionCaptureVersion ||
        !read_u64(bytes, offset, output.frame_number) ||
        !read_u32(bytes, offset, output.width) ||
        !read_u32(bytes, offset, output.height) ||
        !read_u32(bytes, offset, output.stride_bytes) ||
        !read_u64(bytes, offset, pixel_bytes) ||
        !read_u64(bytes, offset, depth_count)) {
        output = {};
        return false;
    }

    const std::uint64_t expected_depth_count = static_cast<std::uint64_t>(output.width) * output.height;
    const std::uint64_t expected_pixel_bytes = static_cast<std::uint64_t>(output.stride_bytes) * output.height;
    if (output.width == 0 || output.height == 0 || output.stride_bytes < output.width * 4u ||
        depth_count != expected_depth_count || pixel_bytes != expected_pixel_bytes ||
        depth_count > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
        output = {};
        return false;
    }

    const std::uint64_t payload_bytes = pixel_bytes + depth_count * sizeof(float);
    if (offset + payload_bytes != checksum_offset) {
        output = {};
        return false;
    }

    output.pixels.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                         bytes.begin() + static_cast<std::ptrdiff_t>(offset + pixel_bytes));
    offset += static_cast<std::size_t>(pixel_bytes);
    output.depth_m.resize(static_cast<std::size_t>(depth_count));
    for (float& value : output.depth_m) {
        std::uint32_t bits{};
        if (!read_u32(bytes, offset, bits)) {
            output = {};
            return false;
        }
        value = std::bit_cast<float>(bits);
    }
    return true;
}

} // namespace area51xr
