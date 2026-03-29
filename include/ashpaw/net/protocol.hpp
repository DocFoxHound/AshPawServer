#pragma once

#include "ashpaw/world/world.hpp"

#include <span>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ashpaw::net {

enum class Opcode : std::uint8_t {
    client_hello = 1,
    server_hello = 2,
    join_accepted = 3,
    join_rejected = 4,
    movement_input = 5,
    player_spawn = 6,
    player_despawn = 7,
    transform_snapshot = 8,
    interaction_request = 9,
    interaction_result = 10,
    object_state_update = 11,
    chat_send = 12,
    chat_broadcast = 13,
    identity_update = 14
};

enum class RejectReason : std::uint8_t {
    invalid_protocol = 1,
    server_full = 2,
    malformed_packet = 3
};

struct ClientHello {
    std::uint16_t protocol_version {1};
    std::string display_name;
    std::string local_map_id;
    std::string local_package_version;
    std::string local_content_hash;
};

struct ServerHello {
    std::uint16_t protocol_version {1};
    std::uint16_t tick_rate {20};
    std::string map_id;
    std::string package_version;
    std::string content_hash;
    bool package_download_required {false};
};

struct JoinAccepted {
    std::uint32_t session_id {0};
    std::uint32_t entity_id {0};
    float spawn_x {0.0F};
    float spawn_y {0.0F};
    std::int32_t spawn_z {0};
};

struct JoinRejected {
    RejectReason reason {RejectReason::malformed_packet};
    std::string message;
};

struct MovementInput {
    std::int8_t move_x {0};
    std::int8_t move_y {0};
};

struct PlayerSpawn {
    std::uint32_t entity_id {0};
    float x {0.0F};
    float y {0.0F};
    std::int32_t z {0};
};

struct PlayerDespawn {
    std::uint32_t entity_id {0};
};

struct InteractionRequest {
    std::string target_id;
};

struct ChatSend {
    std::string message;
};

struct ChatBroadcast {
    std::uint32_t entity_id {0};
    std::string display_name;
    std::string message;
};

struct IdentityUpdate {
    std::uint32_t entity_id {0};
    std::string display_name;
};

struct InteractionResultPacket {
    world::InteractionStatus status {world::InteractionStatus::invalid_target};
    std::string target_id;
    std::string message;
};

struct ObjectStateUpdate {
    std::string target_id;
    bool is_open {false};
    std::uint32_t occupant_entity_id {0};
};

struct TransformSnapshotEntry {
    std::uint32_t entity_id {0};
    float x {0.0F};
    float y {0.0F};
    std::int32_t z {0};
};

struct TransformSnapshot {
    std::vector<TransformSnapshotEntry> entries;
};

struct IncomingPacket {
    Opcode opcode;
    std::vector<std::uint8_t> payload;
};

constexpr std::uint16_t kProtocolVersion = 1;

[[nodiscard]] std::optional<IncomingPacket> decode_header(const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::optional<ClientHello> decode_client_hello(std::span<const std::uint8_t> payload);
[[nodiscard]] std::optional<MovementInput> decode_movement_input(std::span<const std::uint8_t> payload);
[[nodiscard]] std::optional<InteractionRequest> decode_interaction_request(std::span<const std::uint8_t> payload);
[[nodiscard]] std::optional<ChatSend> decode_chat_send(std::span<const std::uint8_t> payload);
[[nodiscard]] std::optional<TransformSnapshot> decode_transform_snapshot(std::span<const std::uint8_t> payload);

[[nodiscard]] std::vector<std::uint8_t> encode_client_hello(const ClientHello& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_server_hello(const ServerHello& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_join_accepted(const JoinAccepted& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_join_rejected(const JoinRejected& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_movement_input(const MovementInput& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_player_spawn(const PlayerSpawn& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_player_despawn(const PlayerDespawn& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_interaction_request(const InteractionRequest& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_interaction_result(const InteractionResultPacket& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_object_state_update(const ObjectStateUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_chat_send(const ChatSend& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_chat_broadcast(const ChatBroadcast& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_identity_update(const IdentityUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> encode_transform_snapshot(const TransformSnapshot& packet);

TransformSnapshot snapshot_from_world(const std::vector<world::EntityState>& entities);

}  // namespace ashpaw::net
