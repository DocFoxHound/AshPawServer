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
    std::int32_t z {0};
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
    std::int32_t z {0};
};

struct MapLevel {
    std::string id;
    std::string name;
    std::int32_t z {0};
    std::size_t width {0};
    std::size_t height {0};
    std::vector<std::string> terrain;
    std::vector<std::string> walls;
};

struct PlacedObject {
    std::string id;
    std::string kind;
    Vec2 position;
    std::int32_t z {0};
    std::string label;
    std::string text;
    bool blocks_movement {false};
};

struct TransitionEndpoint {
    std::string map_id;
    Vec2 position;
    std::int32_t z {0};
};

enum class TransitionType {
    stairs,
    ladder,
    elevator,
    portal,
    unknown
};

struct TransitionLink {
    std::string id;
    TransitionType type {TransitionType::unknown};
    std::string label;
    TransitionEndpoint from;
    TransitionEndpoint to;
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
    std::uint32_t schema_version {1};
    std::string package_version;
    std::string content_hash;
    std::size_t width {0};
    std::size_t height {0};
    std::vector<std::string> collision;
    std::vector<MapLevel> levels;
    std::vector<SpawnPoint> spawn_points;
    std::vector<InteractableObject> interactables;
    std::vector<PlacedObject> objects;
    std::vector<TransitionLink> transitions;

    [[nodiscard]] bool is_blocked(Vec2 position, std::int32_t z = 0) const;
    [[nodiscard]] const SpawnPoint& spawn_point_for_index(std::size_t index) const;
    [[nodiscard]] const MapLevel* level_for_z(std::int32_t z) const;
};

[[nodiscard]] MapData load_map(const std::filesystem::path& path);

}  // namespace ashpaw::world
