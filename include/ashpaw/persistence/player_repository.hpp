#pragma once

#include "ashpaw/world/map.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ashpaw::persistence {

struct PlayerSave {
    std::uint32_t schema_version {1};
    std::string player_id;
    std::string display_name;
    std::string last_map;
    world::Vec2 last_position {};
};

class FilePlayerRepository {
  public:
    explicit FilePlayerRepository(std::filesystem::path root_directory);

    [[nodiscard]] const std::filesystem::path& root_directory() const noexcept;
    [[nodiscard]] std::string normalize_player_id(std::string_view display_name) const;
    [[nodiscard]] std::optional<PlayerSave> load(std::string_view player_id) const;
    bool save(const PlayerSave& save) const;

  private:
    [[nodiscard]] std::filesystem::path path_for_player(std::string_view player_id) const;
    [[nodiscard]] std::filesystem::path backup_path_for_player(std::string_view player_id) const;

    std::filesystem::path root_directory_;
};

}  // namespace ashpaw::persistence
