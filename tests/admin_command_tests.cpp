#include "ashpaw/app/admin_command_processor.hpp"
#include "ashpaw/net/network_server.hpp"
#include "ashpaw/session/session_manager.hpp"
#include "ashpaw/world/map_validation.hpp"
#include "ashpaw/world/world.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

ashpaw::world::MapData admin_map() {
    return ashpaw::world::MapData {
        .map_id = "admin",
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
            {"two", {128.0F, 64.0F}}
        },
        .interactables = {
            {"sign_one", ashpaw::world::InteractableType::sign, {96.0F, 96.0F}, false, false, std::nullopt, "hello"}
        }
    };
}

}  // namespace

TEST_CASE("admin commands can list inspect teleport and stop", "[admin]") {
    ashpaw::config::ServerConfig config;
    ashpaw::world::World world(admin_map());
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer network(config, world, sessions);
    ashpaw::app::RuntimeMetrics metrics {
        .simulation_steps = 12,
        .last_tick_ms = 0.25,
        .max_tick_ms = 0.5,
        .snapshot_broadcasts = 8
    };
    bool stop_requested = false;

    auto& session = sessions.create(nullptr);
    session.state = ashpaw::session::ConnectionState::active;
    session.display_name = "Birch";
    session.player_id = "birch";
    session.entity_id = world.spawn_player();

    ashpaw::app::AdminCommandProcessor processor(
        world,
        sessions,
        network,
        metrics,
        [&stop_requested]() { stop_requested = true; });

    const auto players = processor.execute("players");
    CHECK(players.recognized);
    CHECK(players.success);
    CHECK(players.message.find("Birch") != std::string::npos);

    const auto inspect_entity = processor.execute("inspect 1");
    CHECK(inspect_entity.success);
    CHECK(inspect_entity.message.find("entity=1") != std::string::npos);

    const auto inspect_object = processor.execute("inspect sign_one");
    CHECK(inspect_object.success);
    CHECK(inspect_object.message.find("interactable=sign_one") != std::string::npos);

    const auto teleport = processor.execute("teleport Birch 160 64");
    CHECK(teleport.success);
    const auto entity = world.entity(session.entity_id);
    REQUIRE(entity.has_value());
    CHECK(entity->position.x == 160.0F);
    CHECK(entity->position.y == 64.0F);

    const auto metrics_result = processor.execute("metrics");
    CHECK(metrics_result.success);
    CHECK(metrics_result.message.find("active_players=1") != std::string::npos);
    CHECK(metrics_result.message.find("steps=12") != std::string::npos);

    const auto stop = processor.execute("stop");
    CHECK(stop.success);
    CHECK(stop.stop_requested);
    CHECK(stop_requested);
}

TEST_CASE("admin commands validate targets and map issues", "[admin]") {
    ashpaw::config::ServerConfig config;
    auto map = admin_map();
    map.interactables.push_back({"sign_one", ashpaw::world::InteractableType::sign, {700.0F, 700.0F}, false, false, std::nullopt, {}});

    ashpaw::world::World world(map);
    ashpaw::session::SessionManager sessions(config.max_players);
    ashpaw::net::NetworkServer network(config, world, sessions);
    ashpaw::app::RuntimeMetrics metrics {};
    ashpaw::app::AdminCommandProcessor processor(world, sessions, network, metrics, []() {});

    const auto kick = processor.execute("kick missing");
    CHECK(kick.recognized);
    CHECK_FALSE(kick.success);

    const auto teleport = processor.execute("teleport missing 10 10");
    CHECK(teleport.recognized);
    CHECK_FALSE(teleport.success);

    const auto validate = processor.execute("validate_map");
    CHECK(validate.recognized);
    CHECK_FALSE(validate.success);
    CHECK(validate.message.find("duplicate interactable id") != std::string::npos);
}

TEST_CASE("map validation catches blocked spawns and duplicate interactables", "[admin]") {
    ashpaw::world::MapData map {
        .map_id = "broken",
        .tile_size = 32,
        .width = 4,
        .height = 4,
        .collision = {
            "####",
            "#..#",
            "#..#",
            "####"
        },
        .spawn_points = {
            {"blocked", {0.0F, 0.0F}}
        },
        .interactables = {
            {"crate", ashpaw::world::InteractableType::container, {64.0F, 64.0F}, false, false, std::nullopt, {}},
            {"crate", ashpaw::world::InteractableType::container, {160.0F, 160.0F}, false, false, std::nullopt, {}}
        }
    };

    const auto issues = ashpaw::world::validate_map(map);
    CHECK(issues.size() >= 3);
}
