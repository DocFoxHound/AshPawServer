#include "ashpaw/net/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <span>

namespace ashpaw::net {

namespace {

template <typename T>
void append_scalar(std::vector<std::uint8_t>& bytes, T value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

template <typename T>
std::optional<T> read_scalar(std::span<const std::uint8_t>& payload) {
    if (payload.size() < sizeof(T)) {
        return std::nullopt;
    }
    T value {};
    std::memcpy(&value, payload.data(), sizeof(T));
    payload = payload.subspan(sizeof(T));
    return value;
}

void append_string(std::vector<std::uint8_t>& bytes, std::string_view value) {
    append_scalar<std::uint8_t>(bytes, static_cast<std::uint8_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

std::optional<std::string> read_string(std::span<const std::uint8_t>& payload) {
    const auto length = read_scalar<std::uint8_t>(payload);
    if (!length.has_value() || payload.size() < *length) {
        return std::nullopt;
    }
    std::string value(reinterpret_cast<const char*>(payload.data()), *length);
    payload = payload.subspan(*length);
    return value;
}

std::vector<std::uint8_t> begin_packet(Opcode opcode) {
    return {static_cast<std::uint8_t>(opcode)};
}

}  // namespace

std::optional<IncomingPacket> decode_header(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }

    const auto opcode_value = bytes.front();
    if (opcode_value < static_cast<std::uint8_t>(Opcode::client_hello) ||
        opcode_value > static_cast<std::uint8_t>(Opcode::identity_update)) {
        return std::nullopt;
    }

    return IncomingPacket {
        .opcode = static_cast<Opcode>(opcode_value),
        .payload = std::vector<std::uint8_t>(bytes.begin() + 1, bytes.end())
    };
}

std::optional<ClientHello> decode_client_hello(std::span<const std::uint8_t> payload) {
    ClientHello packet;
    const auto version = read_scalar<std::uint16_t>(payload);
    const auto display_name = read_string(payload);
    if (!version.has_value() || !display_name.has_value()) {
        return std::nullopt;
    }
    packet.protocol_version = *version;
    packet.display_name = *display_name;

    if (!payload.empty()) {
        const auto local_map_id = read_string(payload);
        const auto local_package_version = read_string(payload);
        const auto local_content_hash = read_string(payload);
        if (!local_map_id.has_value() || !local_package_version.has_value() || !local_content_hash.has_value()) {
            return std::nullopt;
        }
        packet.local_map_id = *local_map_id;
        packet.local_package_version = *local_package_version;
        packet.local_content_hash = *local_content_hash;
    }

    if (!payload.empty()) {
        return std::nullopt;
    }
    return packet;
}

std::optional<MovementInput> decode_movement_input(std::span<const std::uint8_t> payload) {
    const auto x = read_scalar<std::int8_t>(payload);
    const auto y = read_scalar<std::int8_t>(payload);
    if (!x.has_value() || !y.has_value() || !payload.empty()) {
        return std::nullopt;
    }

    return MovementInput {
        .move_x = static_cast<std::int8_t>(std::clamp<int>(*x, -1, 1)),
        .move_y = static_cast<std::int8_t>(std::clamp<int>(*y, -1, 1))
    };
}

std::optional<InteractionRequest> decode_interaction_request(std::span<const std::uint8_t> payload) {
    const auto target_id = read_string(payload);
    if (!target_id.has_value() || !payload.empty() || target_id->empty()) {
        return std::nullopt;
    }
    return InteractionRequest {.target_id = *target_id};
}

std::optional<ChatSend> decode_chat_send(std::span<const std::uint8_t> payload) {
    const auto message = read_string(payload);
    if (!message.has_value() || !payload.empty() || message->empty()) {
        return std::nullopt;
    }
    return ChatSend {.message = *message};
}

std::optional<TransformSnapshot> decode_transform_snapshot(std::span<const std::uint8_t> payload) {
    TransformSnapshot snapshot;
    const auto entity_count = read_scalar<std::uint16_t>(payload);
    if (!entity_count.has_value()) {
        return std::nullopt;
    }

    snapshot.entries.reserve(*entity_count);
    for (std::uint16_t i = 0; i < *entity_count; ++i) {
        const auto entity_id = read_scalar<std::uint32_t>(payload);
        const auto x = read_scalar<float>(payload);
        const auto y = read_scalar<float>(payload);
        const auto z = read_scalar<std::int32_t>(payload);
        if (!entity_id.has_value() || !x.has_value() || !y.has_value() || !z.has_value()) {
            return std::nullopt;
        }
        snapshot.entries.push_back(TransformSnapshotEntry {
            .entity_id = *entity_id,
            .x = *x,
            .y = *y,
            .z = *z
        });
    }

    if (!payload.empty()) {
        return std::nullopt;
    }
    return snapshot;
}

std::vector<std::uint8_t> encode_client_hello(const ClientHello& packet) {
    auto bytes = begin_packet(Opcode::client_hello);
    append_scalar<std::uint16_t>(bytes, packet.protocol_version);
    append_string(bytes, packet.display_name);
    append_string(bytes, packet.local_map_id);
    append_string(bytes, packet.local_package_version);
    append_string(bytes, packet.local_content_hash);
    return bytes;
}

std::vector<std::uint8_t> encode_server_hello(const ServerHello& packet) {
    auto bytes = begin_packet(Opcode::server_hello);
    append_scalar<std::uint16_t>(bytes, packet.protocol_version);
    append_scalar<std::uint16_t>(bytes, packet.tick_rate);
    append_string(bytes, packet.map_id);
    append_string(bytes, packet.package_version);
    append_string(bytes, packet.content_hash);
    append_scalar<std::uint8_t>(bytes, packet.package_download_required ? 1U : 0U);
    return bytes;
}

std::vector<std::uint8_t> encode_join_accepted(const JoinAccepted& packet) {
    auto bytes = begin_packet(Opcode::join_accepted);
    append_scalar<std::uint32_t>(bytes, packet.session_id);
    append_scalar<std::uint32_t>(bytes, packet.entity_id);
    append_scalar<float>(bytes, packet.spawn_x);
    append_scalar<float>(bytes, packet.spawn_y);
    append_scalar<std::int32_t>(bytes, packet.spawn_z);
    return bytes;
}

std::vector<std::uint8_t> encode_join_rejected(const JoinRejected& packet) {
    auto bytes = begin_packet(Opcode::join_rejected);
    append_scalar<std::uint8_t>(bytes, static_cast<std::uint8_t>(packet.reason));
    append_string(bytes, packet.message);
    return bytes;
}

std::vector<std::uint8_t> encode_movement_input(const MovementInput& packet) {
    auto bytes = begin_packet(Opcode::movement_input);
    append_scalar<std::int8_t>(bytes, packet.move_x);
    append_scalar<std::int8_t>(bytes, packet.move_y);
    return bytes;
}

std::vector<std::uint8_t> encode_player_spawn(const PlayerSpawn& packet) {
    auto bytes = begin_packet(Opcode::player_spawn);
    append_scalar<std::uint32_t>(bytes, packet.entity_id);
    append_scalar<float>(bytes, packet.x);
    append_scalar<float>(bytes, packet.y);
    append_scalar<std::int32_t>(bytes, packet.z);
    return bytes;
}

std::vector<std::uint8_t> encode_player_despawn(const PlayerDespawn& packet) {
    auto bytes = begin_packet(Opcode::player_despawn);
    append_scalar<std::uint32_t>(bytes, packet.entity_id);
    return bytes;
}

std::vector<std::uint8_t> encode_interaction_request(const InteractionRequest& packet) {
    auto bytes = begin_packet(Opcode::interaction_request);
    append_string(bytes, packet.target_id);
    return bytes;
}

std::vector<std::uint8_t> encode_interaction_result(const InteractionResultPacket& packet) {
    auto bytes = begin_packet(Opcode::interaction_result);
    append_scalar<std::uint8_t>(bytes, static_cast<std::uint8_t>(packet.status));
    append_string(bytes, packet.target_id);
    append_string(bytes, packet.message);
    return bytes;
}

std::vector<std::uint8_t> encode_object_state_update(const ObjectStateUpdate& packet) {
    auto bytes = begin_packet(Opcode::object_state_update);
    append_string(bytes, packet.target_id);
    append_scalar<std::uint8_t>(bytes, packet.is_open ? 1U : 0U);
    append_scalar<std::uint32_t>(bytes, packet.occupant_entity_id);
    return bytes;
}

std::vector<std::uint8_t> encode_chat_send(const ChatSend& packet) {
    auto bytes = begin_packet(Opcode::chat_send);
    append_string(bytes, packet.message);
    return bytes;
}

std::vector<std::uint8_t> encode_chat_broadcast(const ChatBroadcast& packet) {
    auto bytes = begin_packet(Opcode::chat_broadcast);
    append_scalar<std::uint32_t>(bytes, packet.entity_id);
    append_string(bytes, packet.display_name);
    append_string(bytes, packet.message);
    return bytes;
}

std::vector<std::uint8_t> encode_identity_update(const IdentityUpdate& packet) {
    auto bytes = begin_packet(Opcode::identity_update);
    append_scalar<std::uint32_t>(bytes, packet.entity_id);
    append_string(bytes, packet.display_name);
    return bytes;
}

std::vector<std::uint8_t> encode_transform_snapshot(const TransformSnapshot& packet) {
    auto bytes = begin_packet(Opcode::transform_snapshot);
    append_scalar<std::uint16_t>(bytes, static_cast<std::uint16_t>(packet.entries.size()));
    for (const auto& entry : packet.entries) {
        append_scalar<std::uint32_t>(bytes, entry.entity_id);
        append_scalar<float>(bytes, entry.x);
        append_scalar<float>(bytes, entry.y);
        append_scalar<std::int32_t>(bytes, entry.z);
    }
    return bytes;
}

TransformSnapshot snapshot_from_world(const std::vector<world::EntityState>& entities) {
    TransformSnapshot snapshot;
    snapshot.entries.reserve(entities.size());
    for (const auto& entity : entities) {
        snapshot.entries.push_back(TransformSnapshotEntry {
            .entity_id = entity.id,
            .x = entity.position.x,
            .y = entity.position.y,
            .z = entity.z
        });
    }
    return snapshot;
}

}  // namespace ashpaw::net
