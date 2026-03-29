#pragma once

#include "ashpaw/world/world.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

struct _ENetPeer;

namespace ashpaw::session {

struct ReplicatedObjectState {
    bool is_open {false};
    std::uint32_t occupant_entity_id {0};
};

struct ReplicatedEntityState {
    world::Vec2 position {};
    std::int32_t z {0};
};

enum class ConnectionState {
    connected_unverified,
    handshaking,
    active,
    disconnecting
};

struct Session {
    std::uint32_t session_id {0};
    _ENetPeer* peer {nullptr};
    ConnectionState state {ConnectionState::connected_unverified};
    std::string player_id;
    std::string display_name;
    world::EntityId entity_id {0};
    std::unordered_map<world::EntityId, ReplicatedEntityState> last_replicated_positions;
    std::unordered_map<std::string, ReplicatedObjectState> last_replicated_object_states;
};

}  // namespace ashpaw::session
