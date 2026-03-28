#include "ashpaw/persistence/player_repository.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ashpaw::persistence {

namespace {

constexpr std::uint32_t kPlayerSaveSchemaVersion = 1;

std::string normalize_player_id_value(std::string_view display_name) {
    std::string normalized;
    normalized.reserve(display_name.size());

    for (const unsigned char ch : display_name) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
            continue;
        }

        if (ch == '-' || ch == '_') {
            normalized.push_back(static_cast<char>(ch));
        }
    }

    if (normalized.empty()) {
        return "player";
    }

    return normalized;
}

}  // namespace

FilePlayerRepository::FilePlayerRepository(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

const std::filesystem::path& FilePlayerRepository::root_directory() const noexcept {
    return root_directory_;
}

std::string FilePlayerRepository::normalize_player_id(std::string_view display_name) const {
    return normalize_player_id_value(display_name);
}

std::optional<PlayerSave> FilePlayerRepository::load(std::string_view player_id) const {
    for (const auto& path : {path_for_player(player_id), backup_path_for_player(player_id)}) {
        std::ifstream input(path);
        if (!input.is_open()) {
            continue;
        }

        try {
            const auto json = nlohmann::json::parse(input);

            const auto schema_version = json.at("schema_version").get<std::uint32_t>();
            if (schema_version != kPlayerSaveSchemaVersion) {
                spdlog::warn("ignoring player save for '{}' with unsupported schema {}", player_id, schema_version);
                continue;
            }

            return PlayerSave {
                .schema_version = schema_version,
                .player_id = json.at("player_id").get<std::string>(),
                .display_name = json.at("display_name").get<std::string>(),
                .last_map = json.at("last_map").get<std::string>(),
                .last_position = world::Vec2 {
                    .x = json.at("last_position").at("x").get<float>(),
                    .y = json.at("last_position").at("y").get<float>()
                }
            };
        } catch (const std::exception& ex) {
            spdlog::warn("failed to load player save for '{}' from '{}': {}", player_id, path.string(), ex.what());
        }
    }

    return std::nullopt;
}

bool FilePlayerRepository::save(const PlayerSave& save) const {
    try {
        std::filesystem::create_directories(root_directory_);

        const auto final_path = path_for_player(save.player_id);
        const auto backup_path = backup_path_for_player(save.player_id);
        const auto temp_path = final_path.string() + ".tmp";

        nlohmann::json json {
            {"schema_version", save.schema_version},
            {"player_id", save.player_id},
            {"display_name", save.display_name},
            {"last_map", save.last_map},
            {"last_position",
             {
                 {"x", save.last_position.x},
                 {"y", save.last_position.y}
             }}
        };

        {
            std::ofstream output(temp_path, std::ios::trunc);
            if (!output.is_open()) {
                spdlog::warn("failed to open temporary player save file '{}'", temp_path);
                return false;
            }
            output << json.dump(2);
        }

        std::error_code error;
        if (std::filesystem::exists(final_path)) {
            std::filesystem::copy_file(final_path, backup_path, std::filesystem::copy_options::overwrite_existing, error);
            if (error) {
                spdlog::warn("failed to refresh player backup '{}': {}", backup_path.string(), error.message());
                error.clear();
            }
        }

        std::filesystem::rename(temp_path, final_path, error);
        if (error) {
            std::filesystem::remove(final_path, error);
            error.clear();
            std::filesystem::rename(temp_path, final_path, error);
        }

        if (error) {
            spdlog::warn("failed to finalize player save '{}': {}", final_path.string(), error.message());
            std::filesystem::remove(temp_path, error);
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        spdlog::warn("failed to save player '{}': {}", save.player_id, ex.what());
        return false;
    }
}

std::filesystem::path FilePlayerRepository::path_for_player(std::string_view player_id) const {
    return root_directory_ / (normalize_player_id_value(player_id) + ".json");
}

std::filesystem::path FilePlayerRepository::backup_path_for_player(std::string_view player_id) const {
    return root_directory_ / (normalize_player_id_value(player_id) + ".bak.json");
}

}  // namespace ashpaw::persistence
