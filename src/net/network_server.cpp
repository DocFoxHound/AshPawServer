#include "ashpaw/net/network_server.hpp"

#include "ashpaw/net/protocol.hpp"
#include "ashpaw/util/logging.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

namespace ashpaw::net {

namespace {

constexpr enet_uint8 kReliableChannel = 0;
constexpr enet_uint8 kUnreliableChannel = 1;
constexpr enet_uint32 kReliableFlag = ENET_PACKET_FLAG_RELIABLE;

std::vector<std::uint8_t> packet_bytes(const ENetPacket& packet) {
    const auto* begin = packet.data;
    return std::vector<std::uint8_t>(begin, begin + packet.dataLength);
}

bool same_position(world::Vec2 lhs, world::Vec2 rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

float distance_squared(world::Vec2 lhs, world::Vec2 rhs) {
    const auto dx = lhs.x - rhs.x;
    const auto dy = lhs.y - rhs.y;
    return (dx * dx) + (dy * dy);
}

bool same_object_state(const session::ReplicatedObjectState& lhs, const session::ReplicatedObjectState& rhs) {
    return lhs.is_open == rhs.is_open && lhs.occupant_entity_id == rhs.occupant_entity_id;
}

std::string sanitize_display_name(std::string_view value, std::size_t max_length) {
    std::string sanitized;
    sanitized.reserve(std::min(value.size(), max_length));

    for (const unsigned char ch : value) {
        if (sanitized.size() >= max_length) {
            break;
        }

        if (std::isalnum(ch) != 0 || ch == ' ' || ch == '-' || ch == '_') {
            sanitized.push_back(static_cast<char>(ch));
        }
    }

    const auto first = sanitized.find_first_not_of(' ');
    if (first == std::string::npos) {
        return {};
    }
    const auto last = sanitized.find_last_not_of(' ');
    return sanitized.substr(first, last - first + 1);
}

}  // namespace

NetworkServer::NetworkServer(config::ServerConfig config, world::World& world, session::SessionManager& sessions)
    : config_(std::move(config)),
      world_(world),
      sessions_(sessions),
      player_repository_(config_.player_save_dir) {}

NetworkServer::~NetworkServer() {
    shutdown();
}

void NetworkServer::initialize() {
    ENetAddress address {};
    address.host = ENET_HOST_ANY;
    address.port = config_.listen_port;

    host_ = enet_host_create(&address, config_.max_players, 2, 0, 0);
    if (host_ == nullptr) {
        throw std::runtime_error("failed to create ENet host");
    }
}

void NetworkServer::shutdown() {
    for (auto* session : sessions_.active_sessions()) {
        save_session_player_state(*session);
    }

    if (host_ != nullptr) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }
}

void NetworkServer::service(std::chrono::milliseconds timeout) {
    if (host_ == nullptr) {
        return;
    }

    ENetEvent event {};
    while (enet_host_service(host_, &event, static_cast<enet_uint32>(timeout.count())) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                handle_connect(event.peer);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                handle_receive(event.peer, *event.packet, event.channelID);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                handle_disconnect(event.peer);
                break;
            case ENET_EVENT_TYPE_NONE:
                break;
        }
        timeout = std::chrono::milliseconds(0);
    }
}

std::size_t NetworkServer::broadcast_snapshots() {
    std::size_t packets_sent = 0;
    for (auto* session : sessions_.active_sessions()) {
        if (auto snapshot = build_snapshot_bytes_for_session(*session); snapshot.has_value()) {
            send_packet(session->peer, *snapshot, kUnreliableChannel, false);
            ++packets_sent;
            ++metrics_.snapshot_packets_sent;
        }
        for (const auto& packet : build_object_state_bytes_for_session(*session)) {
            send_packet(session->peer, packet, kReliableChannel, true);
            ++metrics_.object_state_packets_sent;
        }
    }
    return packets_sent;
}

std::vector<std::uint8_t> NetworkServer::build_server_hello_bytes() const {
    return encode_server_hello(ServerHello {
        .protocol_version = kProtocolVersion,
        .tick_rate = config_.tick_rate
    });
}

std::vector<std::uint8_t> NetworkServer::build_join_accepted_bytes(const session::Session& session) const {
    const auto entity = world_.entity(session.entity_id);
    if (!entity.has_value()) {
        throw std::runtime_error("cannot build join packet for missing entity");
    }

    return encode_join_accepted(JoinAccepted {
        .session_id = session.session_id,
        .entity_id = session.entity_id,
        .spawn_x = entity->position.x,
        .spawn_y = entity->position.y
    });
}

std::vector<std::uint8_t> NetworkServer::build_spawn_bytes(world::EntityId entity_id) const {
    const auto entity = world_.entity(entity_id);
    if (!entity.has_value()) {
        throw std::runtime_error("cannot build spawn packet for missing entity");
    }

    return encode_player_spawn(PlayerSpawn {
        .entity_id = entity->id,
        .x = entity->position.x,
        .y = entity->position.y
    });
}

std::vector<std::uint8_t> NetworkServer::build_snapshot_bytes() const {
    return encode_transform_snapshot(snapshot_from_world(world_.snapshot_entities()));
}

std::optional<std::vector<std::uint8_t>> NetworkServer::build_snapshot_bytes_for_session(session::Session& session) {
    TransformSnapshot snapshot;
    const auto viewer = world_.entity(session.entity_id);
    if (!viewer.has_value()) {
        return std::nullopt;
    }

    for (const auto& entity : world_.snapshot_entities_near(viewer->position, static_cast<float>(config_.visibility_radius_units))) {
        const auto found = session.last_replicated_positions.find(entity.id);
        if (found == session.last_replicated_positions.end() || !same_position(found->second, entity.position)) {
            snapshot.entries.push_back(TransformSnapshotEntry {
                .entity_id = entity.id,
                .x = entity.position.x,
                .y = entity.position.y
            });
            session.last_replicated_positions[entity.id] = entity.position;
        }
    }

    for (auto it = session.last_replicated_positions.begin(); it != session.last_replicated_positions.end();) {
        const auto entity = world_.entity(it->first);
        if (!entity.has_value() ||
            distance_squared(viewer->position, entity->position) > static_cast<float>(config_.visibility_radius_units * config_.visibility_radius_units)) {
            it = session.last_replicated_positions.erase(it);
        } else {
            ++it;
        }
    }

    if (snapshot.entries.empty()) {
        return std::nullopt;
    }
    metrics_.snapshot_entries_sent += snapshot.entries.size();
    return encode_transform_snapshot(snapshot);
}

std::vector<std::vector<std::uint8_t>> NetworkServer::build_object_state_bytes_for_session(session::Session& session) {
    std::vector<std::vector<std::uint8_t>> packets;
    const auto viewer = world_.entity(session.entity_id);
    if (!viewer.has_value()) {
        return packets;
    }

    for (const auto& object : world_.interactables_near(viewer->position, static_cast<float>(config_.visibility_radius_units))) {
        if (object.type != world::InteractableType::door &&
            object.type != world::InteractableType::seat &&
            object.type != world::InteractableType::container) {
            continue;
        }

        const session::ReplicatedObjectState next_state {
            .is_open = object.is_open,
            .occupant_entity_id = object.occupant_entity_id.value_or(0)
        };
        const auto found = session.last_replicated_object_states.find(object.id);
        if (found != session.last_replicated_object_states.end() && same_object_state(found->second, next_state)) {
            ++metrics_.object_state_packets_suppressed;
            continue;
        }

        session.last_replicated_object_states[object.id] = next_state;
        packets.push_back(encode_object_state_update(ObjectStateUpdate {
            .target_id = object.id,
            .is_open = object.is_open,
            .occupant_entity_id = object.occupant_entity_id.value_or(0)
        }));
    }

    for (auto it = session.last_replicated_object_states.begin(); it != session.last_replicated_object_states.end();) {
        const auto object = world_.interactable(it->first);
        if (!object.has_value() ||
            distance_squared(viewer->position, object->position) > static_cast<float>(config_.visibility_radius_units * config_.visibility_radius_units)) {
            it = session.last_replicated_object_states.erase(it);
        } else {
            ++it;
        }
    }

    return packets;
}

std::vector<std::vector<std::uint8_t>> NetworkServer::build_initial_object_state_bytes(session::Session& session) {
    return build_object_state_bytes_for_session(session);
}

std::size_t NetworkServer::connected_clients() const noexcept {
    return sessions_.size();
}

const ReplicationMetrics& NetworkServer::metrics() const noexcept {
    return metrics_;
}

bool NetworkServer::disconnect_session(session::Session& session, std::string_view reason) {
    if (session.peer == nullptr || host_ == nullptr) {
        return false;
    }

    session.state = session::ConnectionState::disconnecting;
    spdlog::info("disconnecting session {} (entity {}) reason={}", session.session_id, session.entity_id, reason);
    enet_peer_disconnect(session.peer, 0);
    enet_host_flush(host_);
    return true;
}

void NetworkServer::handle_connect(ENetPeer* peer) {
    spdlog::info("peer connected");
    if (sessions_.is_full()) {
        reject_and_disconnect(peer, RejectReason::server_full, "server full");
        return;
    }
    sessions_.create(peer);
}

void NetworkServer::handle_disconnect(ENetPeer* peer) {
    if (auto* session = sessions_.find(peer); session != nullptr) {
        if (session->entity_id != 0) {
            save_session_player_state(*session);
            const auto entity_id = session->entity_id;
            world_.despawn(entity_id);
            broadcast_packet(encode_player_despawn(PlayerDespawn {.entity_id = entity_id}), kReliableChannel, true);
        }
        spdlog::info("peer disconnected; session={}", session->session_id);
        sessions_.remove(peer);
    }
}

void NetworkServer::handle_receive(ENetPeer* peer, const ENetPacket& packet, enet_uint8 channel) {
    if (packet.dataLength == 0 || packet.dataLength > config_.max_packet_size_bytes) {
        reject_and_disconnect(peer, RejectReason::malformed_packet, "packet size out of bounds");
        return;
    }

    auto decoded = decode_header(packet_bytes(packet));
    if (!decoded.has_value()) {
        reject_and_disconnect(peer, RejectReason::malformed_packet, "bad packet header");
        return;
    }

    auto* session = sessions_.find(peer);
    if (session == nullptr) {
        reject_and_disconnect(peer, RejectReason::malformed_packet, "unknown session");
        return;
    }

    if (!validate_channel(decoded->opcode, channel)) {
        reject_session_and_disconnect(*session, RejectReason::malformed_packet, "unexpected channel");
        return;
    }

    switch (decoded->opcode) {
        case Opcode::client_hello:
            handle_client_hello(*session, decoded->payload);
            break;
        case Opcode::movement_input:
            handle_movement_input(*session, decoded->payload);
            break;
        case Opcode::interaction_request:
            handle_interaction_request(*session, decoded->payload);
            break;
        case Opcode::chat_send:
            handle_chat_send(*session, decoded->payload);
            break;
        default:
            reject_and_disconnect(peer, RejectReason::malformed_packet, "unexpected opcode");
            break;
    }
}

bool NetworkServer::validate_channel(Opcode opcode, enet_uint8 channel) const noexcept {
    switch (opcode) {
        case Opcode::movement_input:
            return channel == kUnreliableChannel || channel == kReliableChannel;
        case Opcode::client_hello:
        case Opcode::interaction_request:
        case Opcode::chat_send:
            return channel == kReliableChannel;
        default:
            return false;
    }
}

void NetworkServer::handle_client_hello(session::Session& current_session, std::span<const std::uint8_t> payload) {
    if (current_session.state != ashpaw::session::ConnectionState::connected_unverified) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "duplicate hello");
        return;
    }

    current_session.state = ashpaw::session::ConnectionState::handshaking;
    const auto hello = decode_client_hello(payload);
    if (!hello.has_value()) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "malformed hello");
        return;
    }

    if (hello->protocol_version != kProtocolVersion) {
        reject_session_and_disconnect(current_session, RejectReason::invalid_protocol, "protocol mismatch");
        return;
    }

    const auto sanitized_name = sanitize_display_name(hello->display_name, config_.max_display_name_length);
    current_session.display_name = sanitized_name.empty() ? "player" : sanitized_name;
    current_session.player_id = player_repository_.normalize_player_id(current_session.display_name);

    if (const auto restored = player_repository_.load(current_session.player_id); restored.has_value()) {
        current_session.player_id = restored->player_id;
        current_session.display_name = restored->display_name;
        if (restored->last_map == world_.map().map_id) {
            current_session.entity_id = world_.spawn_player(restored->last_position);
        }
    }

    send_packet(current_session.peer, build_server_hello_bytes(), kReliableChannel, true);

    if (current_session.entity_id == 0) {
        current_session.entity_id = world_.spawn_player();
    }
    current_session.state = ashpaw::session::ConnectionState::active;
    current_session.last_replicated_positions.clear();
    current_session.last_replicated_object_states.clear();

    send_packet(current_session.peer, build_join_accepted_bytes(current_session), kReliableChannel, true);

    for (const auto& entity : world_.snapshot_entities()) {
        send_packet(current_session.peer, encode_player_spawn(PlayerSpawn {
                               .entity_id = entity.id,
                               .x = entity.position.x,
                               .y = entity.position.y
                           }),
                    kReliableChannel,
                    true);
    }
    for (auto* session : sessions_.active_sessions()) {
        if (session->entity_id == 0) {
            continue;
        }
        send_packet(current_session.peer,
                    encode_identity_update(IdentityUpdate {
                        .entity_id = session->entity_id,
                        .display_name = session->display_name
                    }),
                    kReliableChannel,
                    true);
    }
    for (const auto& packet : build_initial_object_state_bytes(current_session)) {
        send_packet(current_session.peer, packet, kReliableChannel, true);
    }

    broadcast_packet(build_spawn_bytes(current_session.entity_id), kReliableChannel, true);
    broadcast_packet(encode_identity_update(IdentityUpdate {
                         .entity_id = current_session.entity_id,
                         .display_name = current_session.display_name
                     }),
                     kReliableChannel,
                     true);
    spdlog::info("session {} activated as entity {}", current_session.session_id, current_session.entity_id);
}

void NetworkServer::handle_movement_input(session::Session& current_session, std::span<const std::uint8_t> payload) {
    if (current_session.state != ashpaw::session::ConnectionState::active || current_session.entity_id == 0) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "inactive session input");
        return;
    }

    const auto input = decode_movement_input(payload);
    if (!input.has_value()) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "malformed movement");
        return;
    }

    world_.set_movement_intent(current_session.entity_id,
                               world::MovementIntent {
                                   .x = input->move_x,
                                   .y = input->move_y
                               });
}

void NetworkServer::handle_interaction_request(session::Session& current_session, std::span<const std::uint8_t> payload) {
    if (current_session.state != ashpaw::session::ConnectionState::active || current_session.entity_id == 0) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "inactive session interaction");
        return;
    }

    const auto request = decode_interaction_request(payload);
    if (!request.has_value()) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "malformed interaction");
        return;
    }

    const auto result = world_.interact(current_session.entity_id, request->target_id);
    send_packet(current_session.peer,
                encode_interaction_result(InteractionResultPacket {
                    .status = result.status,
                    .target_id = result.target_id,
                    .message = result.message
                }),
                kReliableChannel,
                true);

    if (result.state_changed) {
        broadcast_object_state_update(result);
    }
}

void NetworkServer::handle_chat_send(session::Session& current_session, std::span<const std::uint8_t> payload) {
    if (current_session.state != ashpaw::session::ConnectionState::active || current_session.entity_id == 0) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "inactive session chat");
        return;
    }

    const auto chat = decode_chat_send(payload);
    if (!chat.has_value() || chat->message.size() > config_.max_chat_message_length) {
        reject_session_and_disconnect(current_session, RejectReason::malformed_packet, "malformed chat");
        return;
    }

    broadcast_packet(encode_chat_broadcast(ChatBroadcast {
                         .entity_id = current_session.entity_id,
                         .display_name = current_session.display_name,
                         .message = chat->message
                     }),
                     kReliableChannel,
                     true);
}

void NetworkServer::save_session_player_state(const session::Session& session) {
    if (session.player_id.empty() || session.entity_id == 0) {
        return;
    }

    const auto entity = world_.entity(session.entity_id);
    if (!entity.has_value()) {
        return;
    }

    const persistence::PlayerSave save {
        .schema_version = 1,
        .player_id = session.player_id,
        .display_name = session.display_name,
        .last_map = world_.map().map_id,
        .last_position = entity->position
    };
    player_repository_.save(save);
}

void NetworkServer::broadcast_object_state_update(const world::InteractionResult& result) {
    const auto object = world_.interactable(result.target_id);
    if (!object.has_value()) {
        return;
    }

    for (auto* session : sessions_.active_sessions()) {
        const auto viewer = world_.entity(session->entity_id);
        if (!viewer.has_value()) {
            continue;
        }

        if (distance_squared(viewer->position, object->position) >
            static_cast<float>(config_.visibility_radius_units * config_.visibility_radius_units)) {
            ++metrics_.object_state_packets_suppressed;
            continue;
        }

        const session::ReplicatedObjectState next_state {
            .is_open = result.is_open,
            .occupant_entity_id = result.occupant_entity_id.value_or(0)
        };
        const auto found = session->last_replicated_object_states.find(result.target_id);
        if (found != session->last_replicated_object_states.end() && same_object_state(found->second, next_state)) {
            ++metrics_.object_state_packets_suppressed;
            continue;
        }

        session->last_replicated_object_states[result.target_id] = next_state;
        send_packet(session->peer,
                    encode_object_state_update(ObjectStateUpdate {
                        .target_id = result.target_id,
                        .is_open = result.is_open,
                        .occupant_entity_id = result.occupant_entity_id.value_or(0)
                    }),
                    kReliableChannel,
                    true);
        ++metrics_.object_state_packets_sent;
    }
}

void NetworkServer::reject_session_and_disconnect(session::Session& session, RejectReason reason, std::string_view message) {
    session.state = session::ConnectionState::disconnecting;
    reject_and_disconnect(session.peer, reason, message);
}

void NetworkServer::send_packet(ENetPeer* peer, const std::vector<std::uint8_t>& bytes, enet_uint8 channel, bool reliable) {
    if (host_ == nullptr || peer == nullptr || bytes.empty() || bytes.size() > config_.max_packet_size_bytes) {
        return;
    }
    auto* packet = enet_packet_create(bytes.data(),
                                      bytes.size(),
                                      reliable ? kReliableFlag : 0U);
    enet_peer_send(peer, channel, packet);
    enet_host_flush(host_);
}

void NetworkServer::broadcast_packet(const std::vector<std::uint8_t>& bytes, enet_uint8 channel, bool reliable) {
    if (host_ == nullptr || bytes.empty() || bytes.size() > config_.max_packet_size_bytes) {
        return;
    }
    auto* packet = enet_packet_create(bytes.data(),
                                      bytes.size(),
                                      reliable ? kReliableFlag : 0U);
    enet_host_broadcast(host_, channel, packet);
    enet_host_flush(host_);
}

void NetworkServer::reject_and_disconnect(ENetPeer* peer, RejectReason reason, std::string_view message) {
    send_packet(peer, encode_join_rejected(JoinRejected {.reason = reason, .message = std::string(message)}), kReliableChannel, true);
    enet_peer_disconnect(peer, 0);
    if (host_ != nullptr) {
        enet_host_flush(host_);
    }
}

}  // namespace ashpaw::net
