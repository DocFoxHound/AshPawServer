#include "ashpaw/world/world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ashpaw::world {

namespace {

MovementIntent clamp_intent(MovementIntent intent) {
    intent.x = std::clamp<std::int8_t>(intent.x, -1, 1);
    intent.y = std::clamp<std::int8_t>(intent.y, -1, 1);
    return intent;
}

float distance_squared(Vec2 lhs, Vec2 rhs) {
    const auto dx = lhs.x - rhs.x;
    const auto dy = lhs.y - rhs.y;
    return (dx * dx) + (dy * dy);
}

float distance_squared_3d(Vec2 lhs, std::int32_t lhs_z, Vec2 rhs, std::int32_t rhs_z) {
    if (lhs_z != rhs_z) {
        return std::numeric_limits<float>::max();
    }
    return distance_squared(lhs, rhs);
}

constexpr float kInteractionRangeSquared = 80.0F * 80.0F;

bool same_tile(Vec2 lhs, Vec2 rhs, std::size_t tile_size) {
    const auto lhs_x = static_cast<int>(lhs.x / static_cast<float>(tile_size));
    const auto lhs_y = static_cast<int>(lhs.y / static_cast<float>(tile_size));
    const auto rhs_x = static_cast<int>(rhs.x / static_cast<float>(tile_size));
    const auto rhs_y = static_cast<int>(rhs.y / static_cast<float>(tile_size));
    return lhs_x == rhs_x && lhs_y == rhs_y;
}

}  // namespace

World::World(MapData map)
    : map_(std::move(map)) {}

const MapData& World::map() const noexcept {
    return map_;
}

std::size_t World::entity_count() const noexcept {
    return entities_.size();
}

std::size_t World::interactable_count() const noexcept {
    return map_.interactables.size();
}

std::vector<EntityState> World::snapshot_entities() const {
    std::vector<EntityState> snapshot;
    snapshot.reserve(entities_.size());
    for (const auto& [id, entity] : entities_) {
        (void)id;
        snapshot.push_back(entity);
    }
    std::sort(snapshot.begin(), snapshot.end(), [](const EntityState& lhs, const EntityState& rhs) {
        return lhs.id < rhs.id;
    });
    return snapshot;
}

std::vector<EntityState> World::snapshot_entities_near(Vec2 center, float radius_units) const {
    std::vector<EntityState> snapshot;
    const auto radius_squared = radius_units * radius_units;
    const auto viewer = std::find_if(entities_.begin(), entities_.end(), [&](const auto& entry) {
        return entry.second.position.x == center.x && entry.second.position.y == center.y;
    });
    const auto viewer_z = viewer != entities_.end() ? viewer->second.z : 0;
    for (const auto& [id, entity] : entities_) {
        (void)id;
        if (entity.z == viewer_z && distance_squared(center, entity.position) <= radius_squared) {
            snapshot.push_back(entity);
        }
    }
    std::sort(snapshot.begin(), snapshot.end(), [](const EntityState& lhs, const EntityState& rhs) {
        return lhs.id < rhs.id;
    });
    return snapshot;
}

std::optional<EntityState> World::entity(EntityId id) const {
    const auto found = entities_.find(id);
    if (found == entities_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<InteractableObject> World::interactable(std::string_view id) const {
    const auto found = std::find_if(map_.interactables.begin(), map_.interactables.end(), [id](const InteractableObject& object) {
        return object.id == id;
    });
    if (found == map_.interactables.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<TransitionLink> World::transition(std::string_view id) const {
    const auto found = std::find_if(map_.transitions.begin(), map_.transitions.end(), [id](const TransitionLink& transition) {
        return transition.id == id;
    });
    if (found == map_.transitions.end()) {
        return std::nullopt;
    }
    return *found;
}

std::vector<InteractableObject> World::interactables() const {
    return map_.interactables;
}

std::vector<InteractableObject> World::interactables_near(Vec2 center, float radius_units) const {
    std::vector<InteractableObject> visible;
    const auto radius_squared = radius_units * radius_units;
    const auto viewer = std::find_if(entities_.begin(), entities_.end(), [&](const auto& entry) {
        return entry.second.position.x == center.x && entry.second.position.y == center.y;
    });
    const auto viewer_z = viewer != entities_.end() ? viewer->second.z : 0;
    for (const auto& object : map_.interactables) {
        if (object.z == viewer_z && distance_squared(center, object.position) <= radius_squared) {
            visible.push_back(object);
        }
    }
    return visible;
}

EntityId World::spawn_player() {
    const auto& spawn = map_.spawn_point_for_index(next_spawn_index_++);
    return spawn_player(spawn.position, spawn.z);
}

EntityId World::spawn_player(Vec2 position, std::int32_t z) {
    const auto entity_id = next_entity_id();
    const auto resolved = resolve_spawn_position(position, z);
    entities_.emplace(entity_id, EntityState {
                                   .id = entity_id,
                                   .type = EntityType::player,
                                   .position = resolved.position,
                                   .z = resolved.z
                               });
    return entity_id;
}

bool World::despawn(EntityId id) {
    return entities_.erase(id) > 0;
}

bool World::set_movement_intent(EntityId id, MovementIntent intent) {
    const auto found = entities_.find(id);
    if (found == entities_.end()) {
        return false;
    }
    found->second.movement_intent = clamp_intent(intent);
    return true;
}

bool World::set_entity_position(EntityId id, Vec2 position, std::int32_t z) {
    const auto found = entities_.find(id);
    if (found == entities_.end()) {
        return false;
    }

    if (is_position_blocked(position, z) || is_occupied_by_entity(position, z, id)) {
        return false;
    }

    found->second.position = position;
    found->second.z = z;
    found->second.movement_intent = {};
    return true;
}

InteractionResult World::interact(EntityId entity_id, std::string_view target_id) {
    const auto entity_it = entities_.find(entity_id);
    if (entity_it == entities_.end()) {
        return InteractionResult {
            .status = InteractionStatus::invalid_target,
            .target_id = std::string(target_id),
            .message = "missing player entity",
            .occupant_entity_id = std::nullopt
        };
    }

    const auto object_it = std::find_if(map_.interactables.begin(), map_.interactables.end(), [target_id](const InteractableObject& object) {
        return object.id == target_id;
    });
    const auto transition_it = std::find_if(map_.transitions.begin(), map_.transitions.end(), [target_id](const TransitionLink& transition) {
        return transition.id == target_id;
    });
    if (transition_it != map_.transitions.end()) {
        if (distance_squared_3d(entity_it->second.position, entity_it->second.z, transition_it->from.position, transition_it->from.z) >
            kInteractionRangeSquared) {
            return InteractionResult {
                .status = InteractionStatus::out_of_range,
                .target_id = transition_it->id,
                .message = "transition out of range",
                .occupant_entity_id = std::nullopt
            };
        }
        entity_it->second.position = transition_it->to.position;
        entity_it->second.z = transition_it->to.z;
        return InteractionResult {
            .status = InteractionStatus::success,
            .target_id = transition_it->id,
            .message = transition_it->label.empty() ? "transitioned" : transition_it->label,
            .state_changed = true,
            .occupant_entity_id = std::nullopt
        };
    }
    if (object_it == map_.interactables.end()) {
        return InteractionResult {
            .status = InteractionStatus::not_found,
            .target_id = std::string(target_id),
            .message = "interactable not found",
            .occupant_entity_id = std::nullopt
        };
    }

    if (distance_squared_3d(entity_it->second.position, entity_it->second.z, object_it->position, object_it->z) > kInteractionRangeSquared) {
        return InteractionResult {
            .status = InteractionStatus::out_of_range,
            .target_id = object_it->id,
            .message = "target out of range",
            .is_open = object_it->is_open,
            .occupant_entity_id = object_it->occupant_entity_id
        };
    }

    switch (object_it->type) {
        case InteractableType::door:
            if (object_it->is_open && is_occupied_by_entity(object_it->position, object_it->z)) {
                return InteractionResult {
                    .status = InteractionStatus::blocked,
                    .target_id = object_it->id,
                    .message = "doorway is occupied",
                    .is_open = object_it->is_open,
                    .occupant_entity_id = object_it->occupant_entity_id
                };
            }

            object_it->is_open = !object_it->is_open;
            return InteractionResult {
                .status = InteractionStatus::success,
                .target_id = object_it->id,
                .message = object_it->is_open ? "door opened" : "door closed",
                .state_changed = true,
                .is_open = object_it->is_open,
                .occupant_entity_id = object_it->occupant_entity_id
            };
        case InteractableType::sign:
            return InteractionResult {
                .status = InteractionStatus::success,
                .target_id = object_it->id,
                .message = object_it->text.empty() ? "the sign is blank" : object_it->text,
                .is_open = object_it->is_open,
                .occupant_entity_id = object_it->occupant_entity_id
            };
        case InteractableType::seat:
            if (object_it->occupant_entity_id.has_value() && *object_it->occupant_entity_id != entity_id) {
                return InteractionResult {
                    .status = InteractionStatus::blocked,
                    .target_id = object_it->id,
                    .message = "seat is occupied",
                    .is_open = object_it->is_open,
                    .occupant_entity_id = object_it->occupant_entity_id
                };
            }

            if (object_it->occupant_entity_id == entity_id) {
                object_it->occupant_entity_id.reset();
                return InteractionResult {
                    .status = InteractionStatus::success,
                    .target_id = object_it->id,
                    .message = "you stand up",
                    .state_changed = true,
                    .is_open = object_it->is_open,
                    .occupant_entity_id = std::nullopt
                };
            }

            object_it->occupant_entity_id = entity_id;
            return InteractionResult {
                .status = InteractionStatus::success,
                .target_id = object_it->id,
                .message = "you sit down",
                .state_changed = true,
                .is_open = object_it->is_open,
                .occupant_entity_id = object_it->occupant_entity_id
            };
        case InteractableType::container:
            object_it->is_open = !object_it->is_open;
            return InteractionResult {
                .status = InteractionStatus::success,
                .target_id = object_it->id,
                .message = object_it->is_open ? "container opened" : "container closed",
                .state_changed = true,
                .is_open = object_it->is_open,
                .occupant_entity_id = object_it->occupant_entity_id
            };
        case InteractableType::unknown:
            return InteractionResult {
                .status = InteractionStatus::invalid_target,
                .target_id = object_it->id,
                .message = "unsupported interactable",
                .occupant_entity_id = object_it->occupant_entity_id
            };
    }

    return InteractionResult {
        .status = InteractionStatus::invalid_target,
        .target_id = std::string(target_id),
        .message = "unsupported interactable",
        .occupant_entity_id = std::nullopt
    };
}

void World::tick(float delta_seconds) {
    for (auto& [id, entity] : entities_) {
        (void)id;

        const auto intent = clamp_intent(entity.movement_intent);
        Vec2 desired = entity.position;
        desired.x += static_cast<float>(intent.x) * entity.speed_units_per_second * delta_seconds;
        desired.y += static_cast<float>(intent.y) * entity.speed_units_per_second * delta_seconds;

        if (!is_position_blocked({desired.x, entity.position.y}, entity.z)) {
            entity.position.x = desired.x;
        }
        if (!is_position_blocked({entity.position.x, desired.y}, entity.z)) {
            entity.position.y = desired.y;
        }
    }
}

EntityId World::next_entity_id() {
    return next_entity_id_++;
}

EntityState World::resolve_spawn_position(Vec2 requested_position, std::int32_t requested_z) {
    if (can_spawn_at(requested_position, requested_z)) {
        return EntityState {.position = requested_position, .z = requested_z};
    }

    for (std::size_t index = 0; index < map_.spawn_points.size(); ++index) {
        const auto& candidate = map_.spawn_point_for_index(next_spawn_index_ + index);
        if (can_spawn_at(candidate.position, candidate.z)) {
            next_spawn_index_ += index + 1;
            return EntityState {.position = candidate.position, .z = candidate.z};
        }
    }

    throw std::runtime_error("map has no valid spawn positions");
}

bool World::can_spawn_at(Vec2 position, std::int32_t z) const {
    return !is_position_blocked(position, z) && !is_occupied_by_entity(position, z);
}

bool World::is_position_blocked(Vec2 position, std::int32_t z) const {
    return map_.is_blocked(position, z) || is_occupied_by_closed_object(position, z);
}

bool World::is_occupied_by_closed_object(Vec2 position, std::int32_t z) const {
    const bool interactable_block = std::any_of(map_.interactables.begin(), map_.interactables.end(), [&](const InteractableObject& object) {
        const bool closed_blocker = object.blocks_movement && !object.is_open;
        const bool occupied_seat = object.type == InteractableType::seat && object.occupant_entity_id.has_value();
        return object.z == z && (closed_blocker || occupied_seat) && same_tile(object.position, position, map_.tile_size);
    });
    if (interactable_block) {
        return true;
    }
    return std::any_of(map_.objects.begin(), map_.objects.end(), [&](const PlacedObject& object) {
        return object.z == z && object.blocks_movement && same_tile(object.position, position, map_.tile_size);
    });
}

bool World::is_occupied_by_entity(Vec2 position, std::int32_t z, EntityId ignore_entity_id) const {
    return std::any_of(entities_.begin(), entities_.end(), [&](const auto& entry) {
        const auto& [entity_id, entity] = entry;
        return entity_id != ignore_entity_id && entity.z == z && same_tile(entity.position, position, map_.tile_size);
    });
}

}  // namespace ashpaw::world
