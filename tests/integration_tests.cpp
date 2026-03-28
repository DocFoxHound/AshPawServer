#include "ashpaw/config/server_config.hpp"
#include "ashpaw/net/network_server.hpp"
#include "ashpaw/net/protocol.hpp"
#include "ashpaw/session/session_manager.hpp"
#include "ashpaw/world/world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <enet/enet.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct EnetGuard {
    EnetGuard() {
        REQUIRE(enet_initialize() == 0);
    }

    ~EnetGuard() {
        enet_deinitialize();
    }
};

ashpaw::world::MapData integration_map() {
    return ashpaw::world::MapData {
        .map_id = "integration",
        .tile_size = 32,
        .width = 8,
        .height = 8,
        .collision = {
            "########",
            "#......#",
            "#......#",
            "#......#",
            "#......#",
            "#......#",
            "#......#",
            "########"
        },
        .spawn_points = {
            {"one", {64.0F, 64.0F}},
            {"two", {128.0F, 128.0F}}
        }
    };
}

ashpaw::world::MapData wide_integration_map() {
    return ashpaw::world::MapData {
        .map_id = "wide",
        .tile_size = 32,
        .width = 20,
        .height = 8,
        .collision = {
            "####################",
            "#..................#",
            "#..................#",
            "#..................#",
            "#..................#",
            "#..................#",
            "#..................#",
            "####################"
        },
        .spawn_points = {
            {"near", {64.0F, 64.0F}},
            {"far", {512.0F, 64.0F}}
        }
    };
}

std::vector<std::vector<std::uint8_t>> pump_client_packets(ENetHost* client, int iterations = 20) {
    std::vector<std::vector<std::uint8_t>> packets;
    for (int i = 0; i < iterations; ++i) {
        ENetEvent event {};
        while (enet_host_service(client, &event, 10) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                packets.emplace_back(event.packet->data, event.packet->data + event.packet->dataLength);
                enet_packet_destroy(event.packet);
            }
        }
    }
    return packets;
}

bool wait_for_connect(ashpaw::net::NetworkServer& server, ENetHost* client, int iterations = 100) {
    for (int i = 0; i < iterations; ++i) {
        server.service(std::chrono::milliseconds(1));
        ENetEvent event {};
        if (enet_host_service(client, &event, 10) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            return true;
        }
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

void pump_server(ashpaw::net::NetworkServer& server, int iterations = 10) {
    for (int i = 0; i < iterations; ++i) {
        server.service(std::chrono::milliseconds(5));
        std::this_thread::sleep_for(1ms);
    }
}

bool contains_opcode(const std::vector<std::vector<std::uint8_t>>& packets, ashpaw::net::Opcode opcode) {
    for (const auto& packet : packets) {
        const auto decoded = ashpaw::net::decode_header(packet);
        if (decoded.has_value() && decoded->opcode == opcode) {
            return true;
        }
    }
    return false;
}

class TempDirectory {
  public:
    explicit TempDirectory(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::filesystem::remove_all(path_);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

}  // namespace

TEST_CASE("two clients can handshake, spawn, and receive snapshots", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19001;
    config.tick_rate = 20;
    config.max_players = 4;

    ashpaw::world::World world(integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client_a = enet_host_create(nullptr, 1, 2, 0, 0);
    ENetHost* client_b = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client_a != nullptr);
    REQUIRE(client_b != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer_a = enet_host_connect(client_a, &address, 2, 0);
    ENetPeer* peer_b = enet_host_connect(client_b, &address, 2, 0);
    REQUIRE(peer_a != nullptr);
    REQUIRE(peer_b != nullptr);

    REQUIRE(wait_for_connect(server, client_a));
    REQUIRE(wait_for_connect(server, client_b));

    const auto hello_a = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "A"
    });
    auto* hello_packet_a = enet_packet_create(hello_a.data(), hello_a.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer_a, 0, hello_packet_a);

    const auto hello_b = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "B"
    });
    enet_peer_send(peer_b, 0, enet_packet_create(hello_b.data(), hello_b.size(), ENET_PACKET_FLAG_RELIABLE));

    enet_host_flush(client_a);
    enet_host_flush(client_b);

    pump_server(server, 30);

    const auto packets_a = pump_client_packets(client_a);
    const auto packets_b = pump_client_packets(client_b);

    CHECK(contains_opcode(packets_a, ashpaw::net::Opcode::server_hello));
    CHECK(contains_opcode(packets_a, ashpaw::net::Opcode::join_accepted));
    CHECK(contains_opcode(packets_b, ashpaw::net::Opcode::join_accepted));
    CHECK(contains_opcode(packets_a, ashpaw::net::Opcode::player_spawn));
    CHECK(contains_opcode(packets_b, ashpaw::net::Opcode::player_spawn));
    CHECK(world.entity_count() == 2);

    auto active_sessions = sessions.active_sessions();
    REQUIRE(active_sessions.size() == 2);
    auto initial_snapshot = server.build_snapshot_bytes_for_session(*active_sessions.front());
    REQUIRE(initial_snapshot.has_value());
    const auto initial_snapshot_packet = ashpaw::net::decode_header(*initial_snapshot);
    REQUIRE(initial_snapshot_packet.has_value());
    CHECK(initial_snapshot_packet->opcode == ashpaw::net::Opcode::transform_snapshot);

    auto unchanged_snapshot = server.build_snapshot_bytes_for_session(*active_sessions.front());
    CHECK_FALSE(unchanged_snapshot.has_value());

    const auto move = ashpaw::net::encode_movement_input({.move_x = 1, .move_y = 0});
    enet_peer_send(peer_a, 1, enet_packet_create(move.data(), move.size(), 0));
    enet_host_flush(client_a);

    pump_server(server, 10);
    world.tick(0.05F);
    auto changed_snapshot = server.build_snapshot_bytes_for_session(*active_sessions.front());
    REQUIRE(changed_snapshot.has_value());
    const auto snapshot_packet = ashpaw::net::decode_header(*changed_snapshot);
    REQUIRE(snapshot_packet.has_value());
    CHECK(snapshot_packet->opcode == ashpaw::net::Opcode::transform_snapshot);

    enet_peer_disconnect(peer_a, 0);
    enet_peer_disconnect(peer_b, 0);
    pump_server(server, 20);

    enet_host_destroy(client_a);
    enet_host_destroy(client_b);
    server.shutdown();
}

TEST_CASE("client can request sign interaction and receive authoritative result", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19002;
    config.tick_rate = 20;
    config.max_players = 2;

    auto map = integration_map();
    map.interactables.push_back({
        "sign_one",
        ashpaw::world::InteractableType::sign,
        {64.0F, 64.0F},
        false,
        false,
        std::nullopt,
        "Server-owned sign"
    });

    ashpaw::world::World world(map);
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    REQUIRE(peer != nullptr);
    REQUIRE(wait_for_connect(server, client));

    const auto hello = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "Interactor"
    });
    enet_peer_send(peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);

    pump_server(server, 20);
    (void)pump_client_packets(client);

    const auto interaction = ashpaw::net::encode_interaction_request({
        .target_id = "sign_one"
    });
    enet_peer_send(peer, 0, enet_packet_create(interaction.data(), interaction.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);

    pump_server(server, 10);
    const auto packets = pump_client_packets(client);

    bool saw_result = false;
    for (const auto& packet : packets) {
        const auto decoded = ashpaw::net::decode_header(packet);
        if (decoded.has_value() && decoded->opcode == ashpaw::net::Opcode::interaction_result) {
            saw_result = true;
        }
    }

    CHECK(saw_result);

    enet_peer_disconnect(peer, 0);
    pump_server(server, 10);
    enet_host_destroy(client);
    server.shutdown();
}

TEST_CASE("client receives object state update for seat interaction", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19003;
    config.tick_rate = 20;
    config.max_players = 2;

    auto map = integration_map();
    map.interactables.push_back({
        "seat_one",
        ashpaw::world::InteractableType::seat,
        {64.0F, 64.0F},
        false,
        false,
        std::nullopt,
        {}
    });

    ashpaw::world::World world(map);
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    REQUIRE(peer != nullptr);
    REQUIRE(wait_for_connect(server, client));

    const auto hello = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "Sitter"
    });
    enet_peer_send(peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);

    pump_server(server, 20);
    (void)pump_client_packets(client);

    const auto interaction = ashpaw::net::encode_interaction_request({
        .target_id = "seat_one"
    });
    enet_peer_send(peer, 0, enet_packet_create(interaction.data(), interaction.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);

    pump_server(server, 10);
    const auto packets = pump_client_packets(client);

    bool saw_state_update = false;
    for (const auto& packet : packets) {
        const auto decoded = ashpaw::net::decode_header(packet);
        if (decoded.has_value() && decoded->opcode == ashpaw::net::Opcode::object_state_update) {
            saw_state_update = true;
        }
    }

    CHECK(saw_state_update);

    enet_peer_disconnect(peer, 0);
    pump_server(server, 10);
    enet_host_destroy(client);
    server.shutdown();
}

TEST_CASE("clients receive authoritative identity and chat broadcast", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19005;
    config.tick_rate = 20;
    config.snapshot_rate = 10;
    config.max_players = 2;

    ashpaw::world::World world(integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client_a = enet_host_create(nullptr, 1, 2, 0, 0);
    ENetHost* client_b = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client_a != nullptr);
    REQUIRE(client_b != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer_a = enet_host_connect(client_a, &address, 2, 0);
    ENetPeer* peer_b = enet_host_connect(client_b, &address, 2, 0);
    REQUIRE(peer_a != nullptr);
    REQUIRE(peer_b != nullptr);
    REQUIRE(wait_for_connect(server, client_a));
    REQUIRE(wait_for_connect(server, client_b));

    const auto hello_a = ashpaw::net::encode_client_hello({.protocol_version = ashpaw::net::kProtocolVersion, .display_name = "Birch"});
    const auto hello_b = ashpaw::net::encode_client_hello({.protocol_version = ashpaw::net::kProtocolVersion, .display_name = "Clover"});
    enet_peer_send(peer_a, 0, enet_packet_create(hello_a.data(), hello_a.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_peer_send(peer_b, 0, enet_packet_create(hello_b.data(), hello_b.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client_a);
    enet_host_flush(client_b);

    pump_server(server, 30);
    const auto initial_packets_b = pump_client_packets(client_b);

    bool saw_identity = false;
    for (const auto& packet : initial_packets_b) {
        const auto decoded = ashpaw::net::decode_header(packet);
        if (decoded.has_value() && decoded->opcode == ashpaw::net::Opcode::identity_update) {
            saw_identity = true;
        }
    }
    CHECK(saw_identity);

    const auto chat = ashpaw::net::encode_chat_send({.message = "Hello from Birch"});
    enet_peer_send(peer_a, 0, enet_packet_create(chat.data(), chat.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client_a);

    pump_server(server, 10);
    const auto chat_packets_a = pump_client_packets(client_a);
    const auto chat_packets_b = pump_client_packets(client_b);

    CHECK(contains_opcode(chat_packets_a, ashpaw::net::Opcode::chat_broadcast));
    CHECK(contains_opcode(chat_packets_b, ashpaw::net::Opcode::chat_broadcast));

    enet_peer_disconnect(peer_a, 0);
    enet_peer_disconnect(peer_b, 0);
    pump_server(server, 10);
    enet_host_destroy(client_a);
    enet_host_destroy(client_b);
    server.shutdown();
}

TEST_CASE("snapshot replication respects visibility radius", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19004;
    config.tick_rate = 20;
    config.snapshot_rate = 10;
    config.max_players = 2;
    config.visibility_radius_units = 160;

    ashpaw::world::World world(wide_integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client_a = enet_host_create(nullptr, 1, 2, 0, 0);
    ENetHost* client_b = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client_a != nullptr);
    REQUIRE(client_b != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer_a = enet_host_connect(client_a, &address, 2, 0);
    ENetPeer* peer_b = enet_host_connect(client_b, &address, 2, 0);
    REQUIRE(peer_a != nullptr);
    REQUIRE(peer_b != nullptr);
    REQUIRE(wait_for_connect(server, client_a));
    REQUIRE(wait_for_connect(server, client_b));

    const auto hello_a = ashpaw::net::encode_client_hello({.protocol_version = ashpaw::net::kProtocolVersion, .display_name = "Near"});
    const auto hello_b = ashpaw::net::encode_client_hello({.protocol_version = ashpaw::net::kProtocolVersion, .display_name = "Far"});
    enet_peer_send(peer_a, 0, enet_packet_create(hello_a.data(), hello_a.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_peer_send(peer_b, 0, enet_packet_create(hello_b.data(), hello_b.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client_a);
    enet_host_flush(client_b);
    pump_server(server, 30);

    auto active_sessions = sessions.active_sessions();
    REQUIRE(active_sessions.size() == 2);
    auto near_snapshot = server.build_snapshot_bytes_for_session(*active_sessions[0]);
    auto far_snapshot = server.build_snapshot_bytes_for_session(*active_sessions[1]);
    REQUIRE(near_snapshot.has_value());
    REQUIRE(far_snapshot.has_value());

    const auto near_packet = ashpaw::net::decode_header(*near_snapshot);
    const auto far_packet = ashpaw::net::decode_header(*far_snapshot);
    REQUIRE(near_packet.has_value());
    REQUIRE(far_packet.has_value());
    CHECK(near_packet->opcode == ashpaw::net::Opcode::transform_snapshot);
    CHECK(far_packet->opcode == ashpaw::net::Opcode::transform_snapshot);

    const auto near_entries = ashpaw::net::decode_transform_snapshot(near_packet->payload);
    const auto far_entries = ashpaw::net::decode_transform_snapshot(far_packet->payload);
    REQUIRE(near_entries.has_value());
    REQUIRE(far_entries.has_value());
    CHECK(near_entries->entries.size() == 1);
    CHECK(far_entries->entries.size() == 1);
    CHECK(server.metrics().snapshot_packets_sent == 0);

    enet_peer_disconnect(peer_a, 0);
    enet_peer_disconnect(peer_b, 0);
    pump_server(server, 10);
    enet_host_destroy(client_a);
    enet_host_destroy(client_b);
    server.shutdown();
}

TEST_CASE("player reconnect restores persisted position and identity", "[integration]") {
    EnetGuard enet_guard;
    TempDirectory save_directory("ashpaw_integration_persistence");

    ashpaw::config::ServerConfig config;
    config.listen_port = 19006;
    config.tick_rate = 20;
    config.snapshot_rate = 10;
    config.max_players = 2;
    config.player_save_dir = save_directory.path();

    ashpaw::world::World world(integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    REQUIRE(peer != nullptr);
    REQUIRE(wait_for_connect(server, client));

    const auto hello = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "Birch"
    });
    enet_peer_send(peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);
    pump_server(server, 20);
    (void)pump_client_packets(client);

    auto active_sessions = sessions.active_sessions();
    REQUIRE(active_sessions.size() == 1);
    const auto first_entity_id = active_sessions.front()->entity_id;

    const auto move = ashpaw::net::encode_movement_input({.move_x = 1, .move_y = 0});
    enet_peer_send(peer, 1, enet_packet_create(move.data(), move.size(), 0));
    enet_host_flush(client);
    pump_server(server, 5);
    world.tick(0.5F);

    const auto moved_entity = world.entity(first_entity_id);
    REQUIRE(moved_entity.has_value());
    const auto persisted_position = moved_entity->position;
    CHECK(persisted_position.x > 64.0F);

    enet_peer_disconnect(peer, 0);
    enet_host_flush(client);
    pump_server(server, 20);
    enet_host_destroy(client);
    pump_server(server, 20);

    ENetHost* reconnect_client = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(reconnect_client != nullptr);
    ENetPeer* reconnect_peer = enet_host_connect(reconnect_client, &address, 2, 0);
    REQUIRE(reconnect_peer != nullptr);
    REQUIRE(wait_for_connect(server, reconnect_client));

    enet_peer_send(reconnect_peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(reconnect_client);
    pump_server(server, 20);
    (void)pump_client_packets(reconnect_client);

    active_sessions = sessions.active_sessions();
    REQUIRE(active_sessions.size() == 1);
    CHECK(active_sessions.front()->display_name == "Birch");

    const auto restored_entity = world.entity(active_sessions.front()->entity_id);
    REQUIRE(restored_entity.has_value());
    CHECK(restored_entity->position.x == Catch::Approx(persisted_position.x));
    CHECK(restored_entity->position.y == Catch::Approx(persisted_position.y));

    enet_peer_disconnect(reconnect_peer, 0);
    pump_server(server, 10);
    enet_host_destroy(reconnect_client);
    server.shutdown();
}

TEST_CASE("malformed handshake and wrong-channel reliable packets are rejected safely", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19007;
    config.max_players = 2;
    config.max_display_name_length = 8;
    config.max_packet_size_bytes = 64;

    ashpaw::world::World world(integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    REQUIRE(peer != nullptr);
    REQUIRE(wait_for_connect(server, client));

    const auto hello = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "Birch!@#$%^LongName"
    });
    enet_peer_send(peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);
    pump_server(server, 20);
    (void)pump_client_packets(client);

    auto active_sessions = sessions.active_sessions();
    REQUIRE(active_sessions.size() == 1);
    CHECK(active_sessions.front()->display_name == "BirchLon");

    const auto chat = ashpaw::net::encode_chat_send({.message = "wrong channel"});
    enet_peer_send(peer, 1, enet_packet_create(chat.data(), chat.size(), 0));
    enet_host_flush(client);
    pump_server(server, 20);
    CHECK(sessions.active_sessions().empty());

    enet_host_destroy(client);
    server.shutdown();
}

TEST_CASE("server remains stable across repeated joins and leaves", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19008;
    config.max_players = 2;

    ashpaw::world::World world(integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    for (int iteration = 0; iteration < 5; ++iteration) {
        ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
        REQUIRE(client != nullptr);

        ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
        REQUIRE(peer != nullptr);
        REQUIRE(wait_for_connect(server, client));

        const auto hello = ashpaw::net::encode_client_hello({
            .protocol_version = ashpaw::net::kProtocolVersion,
            .display_name = "Loop" + std::to_string(iteration)
        });
        enet_peer_send(peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
        enet_host_flush(client);
        pump_server(server, 20);
        REQUIRE(sessions.active_sessions().size() == 1);

        enet_peer_disconnect(peer, 0);
        enet_host_flush(client);
        pump_server(server, 20);
        CHECK(sessions.active_sessions().empty());
        CHECK(world.entity_count() == 0);

        enet_host_destroy(client);
        pump_server(server, 5);
    }

    server.shutdown();
}

TEST_CASE("server tolerates delayed servicing and bursty movement traffic", "[integration]") {
    EnetGuard enet_guard;

    ashpaw::config::ServerConfig config;
    config.listen_port = 19009;
    config.tick_rate = 20;
    config.snapshot_rate = 10;
    config.max_players = 2;

    ashpaw::world::World world(integration_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer server(config, world, sessions);
    server.initialize();

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    REQUIRE(client != nullptr);

    ENetAddress address {};
    enet_address_set_host(&address, "127.0.0.1");
    address.port = config.listen_port;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    REQUIRE(peer != nullptr);
    REQUIRE(wait_for_connect(server, client));

    const auto hello = ashpaw::net::encode_client_hello({
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "LagTest"
    });
    enet_peer_send(peer, 0, enet_packet_create(hello.data(), hello.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(client);
    pump_server(server, 20);
    (void)pump_client_packets(client);

    auto active_sessions = sessions.active_sessions();
    REQUIRE(active_sessions.size() == 1);
    const auto entity_id = active_sessions.front()->entity_id;

    // Simulate bursty input arriving before the server gets to service and tick.
    for (int index = 0; index < 25; ++index) {
        const auto move = ashpaw::net::encode_movement_input({
            .move_x = static_cast<std::int8_t>((index % 2 == 0) ? 1 : -1),
            .move_y = 0
        });
        enet_peer_send(peer, 1, enet_packet_create(move.data(), move.size(), 0));
    }
    enet_host_flush(client);

    std::this_thread::sleep_for(50ms);
    pump_server(server, 20);
    world.tick(0.15F);
    const auto snapshot = server.build_snapshot_bytes_for_session(*active_sessions.front());
    CHECK(snapshot.has_value());

    const auto entity = world.entity(entity_id);
    REQUIRE(entity.has_value());
    CHECK(entity->position.x >= 32.0F);
    CHECK(entity->position.x <= 224.0F);
    CHECK(sessions.active_sessions().size() == 1);

    enet_peer_disconnect(peer, 0);
    enet_host_flush(client);
    pump_server(server, 20);
    enet_host_destroy(client);
    server.shutdown();
}
