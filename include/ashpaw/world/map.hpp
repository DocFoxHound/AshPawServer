#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ashpaw::world {

struct Vec2 {
    float x {0.0F};
    float y {0.0F};
};

struct SpawnPoint {
    std::string name;
    Vec2 position;
};

enum class InteractableType {
    door,
    sign,
    seat,
    container,
    unknown
};

struct InteractableObject {
    std::string id;
    InteractableType type {InteractableType::unknown};
    Vec2 position;
    bool blocks_movement {false};
    bool is_open {false};
    std::optional<std::uint32_t> occupant_entity_id;
    std::string text;
};

enum class InteractionStatus {
    success,
    not_found,
    out_of_range,
    blocked,
    invalid_target
};

struct InteractionResult {
    InteractionStatus status {InteractionStatus::invalid_target};
    std::string target_id;
    std::string message;
    bool state_changed {false};
    bool is_open {false};
    std::optional<std::uint32_t> occupant_entity_id;
};

struct MapData {
    std::string map_id;
    std::size_t tile_size {32};
    std::size_t width {0};
    std::size_t height {0};
    std::vector<std::string> collision;
    std::vector<SpawnPoint> spawn_points;
    std::vector<InteractableObject> interactables;

    [[nodiscard]] bool is_blocked(Vec2 position) const;
    [[nodiscard]] const SpawnPoint& spawn_point_for_index(std::size_t index) const;
};

[[nodiscard]] MapData load_map(const std::filesystem::path& path);

}  // namespace ashpaw::world
