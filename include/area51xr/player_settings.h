#pragma once

#include <filesystem>

namespace area51xr {

inline constexpr int kPlayerSettingsSchemaVersion = 1;

struct PlayerSettings {
    int schema_version{kPlayerSettingsSchemaVersion};
    bool reticle_enabled{};
    bool passthrough_enabled{};
};

enum class SettingsLoadStatus {
    loaded,
    missing,
    outdated,
    corrupt
};

struct SettingsLoadResult {
    PlayerSettings settings{};
    SettingsLoadStatus status{SettingsLoadStatus::missing};
};

class PlayerSettingsStore {
public:
    explicit PlayerSettingsStore(std::filesystem::path path);

    [[nodiscard]] SettingsLoadResult load() const;
    [[nodiscard]] bool save(const PlayerSettings& settings) const;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

    [[nodiscard]] static std::filesystem::path default_path();

private:
    std::filesystem::path path_;
};

} // namespace area51xr
