#include "ashpaw/world/map_validation.hpp"

#include <sstream>
#include <unordered_set>

namespace ashpaw::world {

namespace {

bool is_in_bounds(const MapData& map, Vec2 position) {
    const auto tile_x = static_cast<int>(position.x / static_cast<float>(map.tile_size));
    const auto tile_y = static_cast<int>(position.y / static_cast<float>(map.tile_size));
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(map.width) &&
           tile_y < static_cast<int>(map.height);
}

}  // namespace

std::vector<MapValidationIssue> validate_map(const MapData& map) {
    std::vector<MapValidationIssue> issues;

    if (map.map_id.empty()) {
        issues.push_back({"map_id must not be empty"});
    }
    if (map.tile_size == 0) {
        issues.push_back({"tile_size must be greater than zero"});
    }
    if (map.width == 0 || map.height == 0) {
        issues.push_back({"map dimensions must be greater than zero"});
    }
    if (map.collision.size() != map.height) {
        issues.push_back({"collision row count does not match map height"});
    }

    for (std::size_t row = 0; row < map.collision.size(); ++row) {
        if (map.collision[row].size() != map.width) {
            std::ostringstream message;
            message << "collision row " << row << " does not match map width";
            issues.push_back({message.str()});
        }
    }

    if (map.spawn_points.empty()) {
        issues.push_back({"map must include at least one spawn point"});
    }

    std::unordered_set<std::string> interactable_ids;
    for (const auto& spawn : map.spawn_points) {
        if (!is_in_bounds(map, spawn.position)) {
            issues.push_back({"spawn point '" + spawn.name + "' is out of bounds"});
            continue;
        }
        if (map.is_blocked(spawn.position)) {
            issues.push_back({"spawn point '" + spawn.name + "' is blocked"});
        }
    }

    for (const auto& interactable : map.interactables) {
        if (interactable.id.empty()) {
            issues.push_back({"interactable id must not be empty"});
        } else if (!interactable_ids.insert(interactable.id).second) {
            issues.push_back({"duplicate interactable id '" + interactable.id + "'"});
        }

        if (!is_in_bounds(map, interactable.position)) {
            issues.push_back({"interactable '" + interactable.id + "' is out of bounds"});
        }
    }

    return issues;
}

}  // namespace ashpaw::world
