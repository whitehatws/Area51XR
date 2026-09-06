#include "area51xr/player_settings.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

int main() {
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::temp_directory_path() / ("area51xr-settings-" + unique);
    const auto path = root / "settings-v1.ini";
    area51xr::PlayerSettingsStore store(path);

    const auto missing = store.load();
    assert(missing.status == area51xr::SettingsLoadStatus::missing);
    assert(!missing.settings.reticle_enabled);
    assert(!missing.settings.passthrough_enabled);

    area51xr::PlayerSettings enabled{};
    enabled.reticle_enabled = true;
    enabled.passthrough_enabled = true;
    assert(store.save(enabled));
    const auto loaded = store.load();
    assert(loaded.status == area51xr::SettingsLoadStatus::loaded);
    assert(loaded.settings.reticle_enabled);
    assert(loaded.settings.passthrough_enabled);

    {
        std::ofstream corrupt(path, std::ios::trunc);
        corrupt << "schema_version=1\nreticle_enabled=maybe\n";
    }
    const auto damaged = store.load();
    assert(damaged.status == area51xr::SettingsLoadStatus::corrupt);
    assert(!damaged.settings.reticle_enabled);
    assert(!damaged.settings.passthrough_enabled);

    {
        std::ofstream old(path, std::ios::trunc);
        old << "schema_version=0\nreticle_enabled=1\npassthrough_enabled=1\n";
    }
    const auto outdated = store.load();
    assert(outdated.status == area51xr::SettingsLoadStatus::outdated);
    assert(!outdated.settings.reticle_enabled);
    assert(!outdated.settings.passthrough_enabled);

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 0;
}
