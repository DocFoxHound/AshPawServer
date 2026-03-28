#pragma once

#include "ashpaw/world/map.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ashpaw::world {

using EntityId = std::uint32_t;

enum class EntityType {
    player
};

struct MovementIntent {
    std::int8_t x {0};
    std::int8_t y {0};
};

struct EntityState {
    EntityId id {0};
    EntityType type {EntityType::player};
    Vec2 position {};
    MovementIntent movement_intent {};
    float speed_units_per_second {96.0F};
};

class World {
  public:
    explicit World(MapData map);

    [[nodiscard]] const MapData& map() const noexcept;
    [[nodiscard]] std::size_t entity_count() const noexcept;
    [[nodiscard]] std::size_t interactable_count() const noexcept;
    [[nodiscard]] std::vector<EntityState> snapshot_entities() const;
    [[nodiscard]] std::vector<EntityState> snapshot_entities_near(Vec2 center, float radius_units) const;
    [[nodiscard]] std::optional<EntityState> entity(EntityId id) const;
    [[nodiscard]] std::optional<InteractableObject> interactable(std::string_view id) const;
    [[nodiscard]] std::vector<InteractableObject> interactables() const;
    [[nodiscard]] std::vector<InteractableObject> interactables_near(Vec2 center, float radius_units) const;

    EntityId spawn_player();
    EntityId spawn_player(Vec2 position);
    bool despawn(EntityId id);
    bool set_movement_intent(EntityId id, MovementIntent intent);
    bool set_entity_position(EntityId id, Vec2 position);
    InteractionResult interact(EntityId entity_id, std::string_view target_id);
    void tick(float delta_seconds);

  private:
    [[nodiscard]] EntityId next_entity_id();
    [[nodiscard]] Vec2 resolve_spawn_position(Vec2 requested_position);
    [[nodiscard]] bool can_spawn_at(Vec2 position) const;
    [[nodiscard]] bool is_position_blocked(Vec2 position) const;
    [[nodiscard]] bool is_occupied_by_closed_object(Vec2 position) const;
    [[nodiscard]] bool is_occupied_by_entity(Vec2 position, EntityId ignore_entity_id = 0) const;

    MapData map_;
    EntityId next_entity_id_ {1};
    std::size_t next_spawn_index_ {0};
    std::unordered_map<EntityId, EntityState> entities_;
};

}  // namespace ashpaw::world
