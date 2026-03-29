#include "ashpaw/world/map_validation.hpp"

#include <sstream>
#include <unordered_set>

namespace ashpaw::world {

namespace {

bool is_in_bounds(const MapData& map, Vec2 position, std::int32_t z) {
    const auto* level = map.level_for_z(z);
    if (level == nullptr) {
        return false;
    }
    const auto tile_x = static_cast<int>(position.x / static_cast<float>(map.tile_size));
    const auto tile_y = static_cast<int>(position.y / static_cast<float>(map.tile_size));
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(level->width) &&
           tile_y < static_cast<int>(level->height);
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
    if (map.levels.empty()) {
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
    } else {
        for (const auto& level : map.levels) {
            if (level.width == 0 || level.height == 0) {
                issues.push_back({"map level dimensions must be greater than zero"});
            }
        }
    }

    if (map.spawn_points.empty()) {
        issues.push_back({"map must include at least one spawn point"});
    }

    std::unordered_set<std::string> interactable_ids;
    for (const auto& spawn : map.spawn_points) {
        if (!is_in_bounds(map, spawn.position, spawn.z)) {
            issues.push_back({"spawn point '" + spawn.name + "' is out of bounds"});
            continue;
        }
        if (map.is_blocked(spawn.position, spawn.z)) {
            issues.push_back({"spawn point '" + spawn.name + "' is blocked"});
        }
    }

    for (const auto& interactable : map.interactables) {
        if (interactable.id.empty()) {
            issues.push_back({"interactable id must not be empty"});
        } else if (!interactable_ids.insert(interactable.id).second) {
            issues.push_back({"duplicate interactable id '" + interactable.id + "'"});
        }

        if (!is_in_bounds(map, interactable.position, interactable.z)) {
            issues.push_back({"interactable '" + interactable.id + "' is out of bounds"});
        }
    }

    std::unordered_set<std::string> object_ids;
    for (const auto& object : map.objects) {
        if (object.id.empty()) {
            issues.push_back({"object id must not be empty"});
        } else if (!object_ids.insert(object.id).second) {
            issues.push_back({"duplicate object id '" + object.id + "'"});
        }
        if (!is_in_bounds(map, object.position, object.z)) {
            issues.push_back({"object '" + object.id + "' is out of bounds"});
        }
    }

    std::unordered_set<std::string> transition_ids;
    for (const auto& transition : map.transitions) {
        if (transition.id.empty()) {
            issues.push_back({"transition id must not be empty"});
        } else if (!transition_ids.insert(transition.id).second) {
            issues.push_back({"duplicate transition id '" + transition.id + "'"});
        }
        if (!is_in_bounds(map, transition.from.position, transition.from.z)) {
            issues.push_back({"transition '" + transition.id + "' has out of bounds from endpoint"});
        }
        if (!is_in_bounds(map, transition.to.position, transition.to.z)) {
            issues.push_back({"transition '" + transition.id + "' has out of bounds to endpoint"});
        }
    }

    return issues;
}

}  // namespace ashpaw::world
