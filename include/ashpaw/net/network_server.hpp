#pragma once

#include "ashpaw/config/server_config.hpp"
#include "ashpaw/net/protocol.hpp"
#include "ashpaw/persistence/player_repository.hpp"
#include "ashpaw/session/session_manager.hpp"
#include "ashpaw/world/world.hpp"

#include <enet/enet.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ashpaw::net {

struct ReplicationMetrics {
    std::size_t snapshot_packets_sent {0};
    std::size_t snapshot_entries_sent {0};
    std::size_t object_state_packets_sent {0};
    std::size_t object_state_packets_suppressed {0};
};

class NetworkServer {
  public:
    NetworkServer(config::ServerConfig config, world::World& world, session::SessionManager& sessions);
    ~NetworkServer();

    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;

    void initialize();
    void shutdown();
    void service(std::chrono::milliseconds timeout);
    std::size_t broadcast_snapshots();

    [[nodiscard]] std::vector<std::uint8_t> build_server_hello_bytes() const;
    [[nodiscard]] std::vector<std::uint8_t> build_join_accepted_bytes(const session::Session& session) const;
    [[nodiscard]] std::vector<std::uint8_t> build_spawn_bytes(world::EntityId entity_id) const;
    [[nodiscard]] std::vector<std::uint8_t> build_snapshot_bytes() const;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> build_snapshot_bytes_for_session(session::Session& session);
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> build_object_state_bytes_for_session(session::Session& session);
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> build_initial_object_state_bytes(session::Session& session);
    [[nodiscard]] std::size_t connected_clients() const noexcept;
    [[nodiscard]] const ReplicationMetrics& metrics() const noexcept;
    bool disconnect_session(session::Session& session, std::string_view reason);

  private:
    void handle_connect(ENetPeer* peer);
    void handle_disconnect(ENetPeer* peer);
    void handle_receive(ENetPeer* peer, const ENetPacket& packet, enet_uint8 channel);
    [[nodiscard]] bool validate_channel(Opcode opcode, enet_uint8 channel) const noexcept;
    void handle_client_hello(session::Session& session, std::span<const std::uint8_t> payload);
    void handle_movement_input(session::Session& session, std::span<const std::uint8_t> payload);
    void handle_interaction_request(session::Session& session, std::span<const std::uint8_t> payload);
    void handle_chat_send(session::Session& session, std::span<const std::uint8_t> payload);
    void save_session_player_state(const session::Session& session);
    void broadcast_object_state_update(const world::InteractionResult& result);
    void send_packet(ENetPeer* peer, const std::vector<std::uint8_t>& bytes, enet_uint8 channel, bool reliable);
    void broadcast_packet(const std::vector<std::uint8_t>& bytes, enet_uint8 channel, bool reliable);
    void reject_session_and_disconnect(session::Session& session, RejectReason reason, std::string_view message);
    void reject_and_disconnect(ENetPeer* peer, RejectReason reason, std::string_view message);

    config::ServerConfig config_;
    world::World& world_;
    session::SessionManager& sessions_;
    persistence::FilePlayerRepository player_repository_;
    ReplicationMetrics metrics_ {};
    ENetHost* host_ {nullptr};
};

}  // namespace ashpaw::net
