#include "area51xr/player_settings.h"

#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace area51xr {
namespace {

std::optional<bool> parse_bool(const std::string& value) {
    if (value == "0" || value == "false") return false;
    if (value == "1" || value == "true") return true;
    return std::nullopt;
}

} // namespace

PlayerSettingsStore::PlayerSettingsStore(std::filesystem::path path)
    : path_(std::move(path)) {}

SettingsLoadResult PlayerSettingsStore::load() const {
    std::ifstream input(path_);
    if (!input) return {};

    PlayerSettings parsed{};
    bool saw_schema = false;
    bool invalid = false;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos || equals == 0 || equals + 1 >= line.size()) {
            invalid = true;
            break;
        }
        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);
        if (key == "schema_version") {
            if (saw_schema) { invalid = true; break; }
            saw_schema = true;
            try {
                std::size_t used{};
                parsed.schema_version = std::stoi(value, &used);
                if (used != value.size()) invalid = true;
            } catch (...) {
                invalid = true;
            }
        } else if (key == "reticle_enabled") {
            const auto setting = parse_bool(value);
            if (!setting) { invalid = true; break; }
            parsed.reticle_enabled = *setting;
        } else if (key == "passthrough_enabled") {
            const auto setting = parse_bool(value);
            if (!setting) { invalid = true; break; }
            parsed.passthrough_enabled = *setting;
        }
    }

    if (!input.eof() && input.fail()) invalid = true;
    if (invalid || !saw_schema)
        return {PlayerSettings{}, SettingsLoadStatus::corrupt};
    if (parsed.schema_version != kPlayerSettingsSchemaVersion)
        return {PlayerSettings{}, SettingsLoadStatus::outdated};
    return {parsed, SettingsLoadStatus::loaded};
}

bool PlayerSettingsStore::save(const PlayerSettings& settings) const {
    std::error_code error;
    const auto parent = path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) return false;
    }

    auto temporary = path_;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) return false;
        output << "# Area51XR player preferences\n"
               << "schema_version=" << kPlayerSettingsSchemaVersion << '\n'
               << "reticle_enabled=" << (settings.reticle_enabled ? 1 : 0) << '\n'
               << "passthrough_enabled=" << (settings.passthrough_enabled ? 1 : 0) << '\n';
        output.flush();
        if (!output) return false;
    }

    std::filesystem::remove(path_, error);
    error.clear();
    std::filesystem::rename(temporary, path_, error);
    if (!error) return true;
    std::filesystem::remove(temporary, error);
    return false;
}

const std::filesystem::path& PlayerSettingsStore::path() const noexcept { return path_; }

std::filesystem::path PlayerSettingsStore::default_path() {
    if (const char* root = std::getenv("A51XR_DATA_ROOT"); root && *root)
        return std::filesystem::path(root) / "settings-v1.ini";
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local)
        return std::filesystem::path(local) / "Area51XR" / "settings-v1.ini";
    return std::filesystem::current_path() / "Area51XR" / "settings-v1.ini";
}

} // namespace area51xr
