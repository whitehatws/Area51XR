#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace area51xr_mame {

constexpr std::uint32_t kProtocolVersion = 1;
constexpr std::size_t kFrameBufferBytes = 760u * 512u * 4u;

struct GunState {
    std::uint32_t protocol_version;
    std::uint32_t padding0;
    std::uint64_t sequence;
    float aim_x;
    float aim_y;
    std::uint8_t trigger;
    std::uint8_t reserved[7];
};

struct FrameHeader {
    std::uint32_t protocol_version;
    std::uint32_t padding0;
    std::uint64_t frame_number;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride_bytes;
    std::uint32_t pixel_format;
    std::uint64_t presentation_time_ns;
    std::uint64_t payload_bytes;
};

struct SharedState {
    std::uint32_t protocol_version;
    std::uint32_t reserved0;
    GunState gun;
    FrameHeader frame;
    std::uint8_t frame_pixels[kFrameBufferBytes];
};

static_assert(sizeof(GunState) == 32);
static_assert(sizeof(FrameHeader) == 48);
static_assert(offsetof(SharedState, gun) == 8);
static_assert(offsetof(SharedState, frame) == 40);
static_assert(offsetof(SharedState, frame_pixels) == 88);

class Bridge {
public:
    ~Bridge() { close(); }

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    static Bridge& instance()
    {
        static Bridge bridge;
        return bridge;
    }

    const GunState* gun()
    {
        if (!ensure_open())
            return nullptr;
        if (state_->protocol_version != kProtocolVersion || state_->gun.protocol_version != kProtocolVersion)
            return nullptr;
        return &state_->gun;
    }

    template <typename Bitmap>
    void publish_bitmap(const Bitmap& bitmap)
    {
        if (!ensure_open())
            return;

        const std::uint32_t width = static_cast<std::uint32_t>(bitmap.width());
        const std::uint32_t height = static_cast<std::uint32_t>(bitmap.height());
        const std::size_t row_bytes = static_cast<std::size_t>(width) * sizeof(std::uint32_t);
        const std::size_t payload_bytes = row_bytes * height;
        if (payload_bytes > kFrameBufferBytes)
            return;

        for (std::uint32_t y = 0; y < height; ++y)
            std::memcpy(state_->frame_pixels + static_cast<std::size_t>(y) * row_bytes, &bitmap.pix(y), row_bytes);

        FrameHeader header{};
        header.protocol_version = kProtocolVersion;
        header.frame_number = ++frame_number_;
        header.width = width;
        header.height = height;
        header.stride_bytes = static_cast<std::uint32_t>(row_bytes);
        header.pixel_format = 1; // MAME bitmap_rgb32 native 32-bit pixels
        header.payload_bytes = payload_bytes;
        state_->frame = header;
    }

private:
    Bridge() = default;

    bool ensure_open()
    {
#ifdef _WIN32
        if (state_)
            return true;

        mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\Area51XR_MAME_v1");
        if (!mapping_)
            return false;

        void* view = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
        if (!view)
        {
            CloseHandle(mapping_);
            mapping_ = nullptr;
            return false;
        }

        state_ = static_cast<SharedState*>(view);
        if (state_->protocol_version != kProtocolVersion)
        {
            close();
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    void close()
    {
#ifdef _WIN32
        if (state_)
            UnmapViewOfFile(state_);
        if (mapping_)
            CloseHandle(mapping_);
#endif
        state_ = nullptr;
#ifdef _WIN32
        mapping_ = nullptr;
#endif
    }

#ifdef _WIN32
    HANDLE mapping_{};
#endif
    SharedState* state_{};
    std::uint64_t frame_number_{};
};

inline const GunState* gun_state()
{
    return Bridge::instance().gun();
}

template <typename Bitmap>
inline void publish_bitmap(const Bitmap& bitmap)
{
    Bridge::instance().publish_bitmap(bitmap);
}

inline std::uint8_t normalized_to_mame_axis(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
}

} // namespace area51xr_mame
