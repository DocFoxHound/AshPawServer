#include "ashpaw/world/map.hpp"

#include <algorithm>
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

TransitionType parse_transition_type(std::string_view value) {
    if (value == "stairs") {
        return TransitionType::stairs;
    }
    if (value == "ladder") {
        return TransitionType::ladder;
    }
    if (value == "elevator") {
        return TransitionType::elevator;
    }
    if (value == "portal") {
        return TransitionType::portal;
    }
    return TransitionType::unknown;
}

}  // namespace

const MapLevel* MapData::level_for_z(std::int32_t z) const {
    const auto found = std::find_if(levels.begin(), levels.end(), [z](const MapLevel& level) {
        return level.z == z;
    });
    if (found != levels.end()) {
        return &(*found);
    }

    if (levels.empty() && z == 0 && width > 0 && height > 0 && !collision.empty()) {
        static thread_local MapLevel legacy_level;
        legacy_level = MapLevel {
            .id = "legacy_level_0",
            .name = "Ground",
            .z = 0,
            .width = width,
            .height = height,
            .terrain = std::vector<std::string>(height, std::string(width, '.')),
            .walls = collision
        };
        return &legacy_level;
    }

    return nullptr;
}

bool MapData::is_blocked(Vec2 position, std::int32_t z) const {
    const auto* level = level_for_z(z);
    if (level == nullptr) {
        return true;
    }

    const auto tile_x = static_cast<int>(position.x / static_cast<float>(tile_size));
    const auto tile_y = static_cast<int>(position.y / static_cast<float>(tile_size));

    if (tile_x < 0 || tile_y < 0 || tile_x >= static_cast<int>(level->width) || tile_y >= static_cast<int>(level->height)) {
        return true;
    }

    const auto row_index = static_cast<std::size_t>(tile_y);
    const auto column_index = static_cast<std::size_t>(tile_x);
    const bool terrain_blocked =
        row_index < level->terrain.size() && column_index < level->terrain[row_index].size() &&
        level->terrain[row_index][column_index] == '#';
    const bool wall_blocked =
        row_index < level->walls.size() && column_index < level->walls[row_index].size() &&
        level->walls[row_index][column_index] == '#';
    return terrain_blocked || wall_blocked;
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
    map.tile_size = json.value("tile_size", map.tile_size);
    map.schema_version = json.value("schema_version", map.schema_version);
    map.package_version = json.value("package_version", std::string {});
    map.content_hash = json.value("content_hash", std::string {});

    if (json.contains("levels")) {
        for (const auto& entry : json.at("levels")) {
            MapLevel level;
            level.id = entry.value("id", std::string {});
            level.name = entry.value("name", std::string {});
            level.z = entry.value("z", 0);
            level.width = entry.at("width").get<std::size_t>();
            level.height = entry.at("height").get<std::size_t>();
            level.terrain = entry.at("terrain").get<std::vector<std::string>>();
            level.walls = entry.at("walls").get<std::vector<std::string>>();
            if (level.terrain.size() != level.height || level.walls.size() != level.height) {
                throw std::runtime_error("level row count does not match map height");
            }
            for (const auto& row : level.terrain) {
                if (row.size() != level.width) {
                    throw std::runtime_error("terrain column count does not match map width");
                }
            }
            for (const auto& row : level.walls) {
                if (row.size() != level.width) {
                    throw std::runtime_error("wall column count does not match map width");
                }
            }
            map.levels.push_back(std::move(level));
        }
        if (!map.levels.empty()) {
            map.width = map.levels.front().width;
            map.height = map.levels.front().height;
            map.collision = map.levels.front().walls;
        }
    } else {
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
    }

    for (const auto& entry : json.at("spawn_points")) {
        map.spawn_points.push_back(SpawnPoint {
            .name = entry.at("name").get<std::string>(),
            .position = Vec2 {
                .x = entry.at("x").get<float>(),
                .y = entry.at("y").get<float>()
            },
            .z = entry.value("z", 0)
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
                .text = entry.value("text", std::string {}),
                .z = entry.value("z", 0)
            });
        }
    }

    if (json.contains("objects")) {
        for (const auto& entry : json.at("objects")) {
            map.objects.push_back(PlacedObject {
                .id = entry.at("id").get<std::string>(),
                .kind = entry.value("kind", std::string {"object"}),
                .position = Vec2 {
                    .x = entry.at("x").get<float>(),
                    .y = entry.at("y").get<float>()
                },
                .z = entry.value("z", 0),
                .label = entry.value("label", std::string {}),
                .text = entry.value("text", std::string {}),
                .blocks_movement = entry.value("blocks_movement", false)
            });
        }
    }

    if (json.contains("transitions")) {
        for (const auto& entry : json.at("transitions")) {
            const auto type = parse_transition_type(entry.at("type").get<std::string>());
            if (type == TransitionType::unknown) {
                throw std::runtime_error("map contains unsupported transition type");
            }
            const auto& from = entry.at("from");
            const auto& to = entry.at("to");
            map.transitions.push_back(TransitionLink {
                .id = entry.at("id").get<std::string>(),
                .type = type,
                .label = entry.value("label", std::string {}),
                .from = TransitionEndpoint {
                    .map_id = from.value("map_id", map.map_id),
                    .position = Vec2 {
                        .x = from.at("x").get<float>(),
                        .y = from.at("y").get<float>()
                    },
                    .z = from.value("z", 0)
                },
                .to = TransitionEndpoint {
                    .map_id = to.value("map_id", map.map_id),
                    .position = Vec2 {
                        .x = to.at("x").get<float>(),
                        .y = to.at("y").get<float>()
                    },
                    .z = to.value("z", 0)
                }
            });
        }
    }

    return map;
}

}  // namespace ashpaw::world
