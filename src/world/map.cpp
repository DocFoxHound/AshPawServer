#include "ashpaw/world/map.hpp"

#include <fstream>
#include <optional>
#include <string_view>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace ashpaw::world {

namespace {

InteractableType parse_interactable_type(std::string_view value) {
    if (value == "door") {
        return InteractableType::door;
    }
    if (value == "sign") {
        return InteractableType::sign;
    }
    if (value == "seat") {
        return InteractableType::seat;
    }
    if (value == "container") {
        return InteractableType::container;
    }
    return InteractableType::unknown;
}

}  // namespace

bool MapData::is_blocked(Vec2 position) const {
    const auto tile_x = static_cast<int>(position.x / static_cast<float>(tile_size));
    const auto tile_y = static_cast<int>(position.y / static_cast<float>(tile_size));

    if (tile_x < 0 || tile_y < 0 || tile_x >= static_cast<int>(width) || tile_y >= static_cast<int>(height)) {
        return true;
    }

    return collision[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)] == '#';
}

const SpawnPoint& MapData::spawn_point_for_index(std::size_t index) const {
    if (spawn_points.empty()) {
        throw std::runtime_error("map has no spawn points");
    }
    return spawn_points[index % spawn_points.size()];
}

MapData load_map(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open map: " + path.string());
    }

    const auto json = nlohmann::json::parse(input);

    MapData map;
    map.map_id = json.at("map_id").get<std::string>();
    map.tile_size = json.at("tile_size").get<std::size_t>();
    map.width = json.at("width").get<std::size_t>();
    map.height = json.at("height").get<std::size_t>();
    map.collision = json.at("collision").get<std::vector<std::string>>();

    if (map.collision.size() != map.height) {
        throw std::runtime_error("collision row count does not match map height");
    }

    for (const auto& row : map.collision) {
        if (row.size() != map.width) {
            throw std::runtime_error("collision column count does not match map width");
        }
    }

    for (const auto& entry : json.at("spawn_points")) {
        map.spawn_points.push_back(SpawnPoint {
            .name = entry.at("name").get<std::string>(),
            .position = Vec2 {
                .x = entry.at("x").get<float>(),
                .y = entry.at("y").get<float>()
            }
        });
    }

    if (map.spawn_points.empty()) {
        throw std::runtime_error("map must include at least one spawn point");
    }

    if (json.contains("interactables")) {
        for (const auto& entry : json.at("interactables")) {
            const auto type = parse_interactable_type(entry.at("type").get<std::string>());
            if (type == InteractableType::unknown) {
                throw std::runtime_error("map contains unsupported interactable type");
            }

            map.interactables.push_back(InteractableObject {
                .id = entry.at("id").get<std::string>(),
                .type = type,
                .position = Vec2 {
                    .x = entry.at("x").get<float>(),
                    .y = entry.at("y").get<float>()
                },
                .blocks_movement = entry.value("blocks_movement", type == InteractableType::door),
                .is_open = entry.value("is_open", false),
                .occupant_entity_id = std::nullopt,
                .text = entry.value("text", std::string {})
            });
        }
    }

    return map;
}

}  // namespace ashpaw::world
