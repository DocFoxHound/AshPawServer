#include "ashpaw/simulation/simulation_clock.hpp"
#include "ashpaw/world/map.hpp"
#include "ashpaw/world/world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

ashpaw::world::MapData test_map() {
    return ashpaw::world::MapData {
        .map_id = "test",
        .tile_size = 32,
        .width = 5,
        .height = 5,
        .collision = {
            "#####",
            "#...#",
            "#...#",
            "#...#",
            "#####"
        },
        .spawn_points = {
            {"a", {64.0F, 64.0F}},
            {"b", {96.0F, 96.0F}}
        },
        .interactables = {
            {"door_a", ashpaw::world::InteractableType::door, {64.0F, 32.0F}, true, false, std::nullopt, {}},
            {"sign_a", ashpaw::world::InteractableType::sign, {96.0F, 96.0F}, false, false, std::nullopt, "Hello traveler"},
            {"seat_a", ashpaw::world::InteractableType::seat, {96.0F, 64.0F}, false, false, std::nullopt, {}},
            {"crate_a", ashpaw::world::InteractableType::container, {64.0F, 96.0F}, false, false, std::nullopt, {}}
        }
    };
}

}  // namespace

TEST_CASE("world spawns and despawns players", "[world]") {
    ashpaw::world::World world(test_map());

    const auto first = world.spawn_player();
    const auto second = world.spawn_player();

    CHECK(first != second);
    CHECK(world.entity_count() == 2);
    CHECK(world.despawn(first));
    CHECK(world.entity_count() == 1);
    CHECK(world.interactable_count() == 4);
}

TEST_CASE("world movement obeys collision and staged intent", "[world]") {
    ashpaw::world::World world(test_map());
    const auto entity_id = world.spawn_player();

    REQUIRE(world.set_movement_intent(entity_id, {.x = 1, .y = 0}));
    world.tick(0.5F);

    const auto moved = world.entity(entity_id);
    REQUIRE(moved.has_value());
    CHECK(moved->position.x > 64.0F);

    REQUIRE(world.set_movement_intent(entity_id, {.x = -1, .y = -1}));
    world.tick(2.0F);

    const auto blocked = world.entity(entity_id);
    REQUIRE(blocked.has_value());
    CHECK(blocked->position.x >= 32.0F);
    CHECK(blocked->position.y >= 32.0F);
}

TEST_CASE("simulation clock accumulates fixed steps", "[simulation]") {
    ashpaw::simulation::SimulationClock clock(20);

    clock.add_elapsed(std::chrono::milliseconds(120));

    CHECK(clock.should_step());
    CHECK(clock.pending_steps() == 2);
    CHECK(clock.step_seconds() == Catch::Approx(0.05F));
}

TEST_CASE("world interactions validate range and mutate door state", "[world]") {
    ashpaw::world::World world(test_map());
    const auto entity_id = world.spawn_player();

    const auto door_result = world.interact(entity_id, "door_a");
    CHECK(door_result.status == ashpaw::world::InteractionStatus::success);
    CHECK(door_result.state_changed);
    CHECK(door_result.is_open);

    const auto sign_result = world.interact(entity_id, "sign_a");
    CHECK(sign_result.status == ashpaw::world::InteractionStatus::success);
    CHECK(sign_result.message == "Hello traveler");

    const auto far_result = world.interact(entity_id, "missing_object");
    CHECK(far_result.status == ashpaw::world::InteractionStatus::not_found);
}

TEST_CASE("closed doors block movement until the server opens them", "[world]") {
    ashpaw::world::World world(test_map());
    const auto entity_id = world.spawn_player();

    REQUIRE(world.set_movement_intent(entity_id, {.x = 0, .y = -1}));
    world.tick(0.2F);

    auto blocked = world.entity(entity_id);
    REQUIRE(blocked.has_value());
    CHECK(blocked->position.y == Catch::Approx(64.0F));

    const auto open_result = world.interact(entity_id, "door_a");
    REQUIRE(open_result.status == ashpaw::world::InteractionStatus::success);
    REQUIRE(open_result.is_open);

    REQUIRE(world.set_movement_intent(entity_id, {.x = 0, .y = -1}));
    world.tick(0.2F);

    auto moved = world.entity(entity_id);
    REQUIRE(moved.has_value());
    CHECK(moved->position.y < 64.0F);

    const auto blocked_close = world.interact(entity_id, "door_a");
    CHECK(blocked_close.status == ashpaw::world::InteractionStatus::blocked);
    CHECK(blocked_close.is_open);
}

TEST_CASE("seats track occupant state authoritatively", "[world]") {
    ashpaw::world::World world(test_map());
    const auto first = world.spawn_player();
    const auto second = world.spawn_player();

    auto sit = world.interact(first, "seat_a");
    CHECK(sit.status == ashpaw::world::InteractionStatus::success);
    CHECK(sit.state_changed);
    REQUIRE(sit.occupant_entity_id.has_value());
    CHECK(*sit.occupant_entity_id == first);

    auto blocked = world.interact(second, "seat_a");
    CHECK(blocked.status == ashpaw::world::InteractionStatus::blocked);

    auto stand = world.interact(first, "seat_a");
    CHECK(stand.status == ashpaw::world::InteractionStatus::success);
    CHECK(stand.state_changed);
    CHECK_FALSE(stand.occupant_entity_id.has_value());
}

TEST_CASE("containers toggle authoritative open state", "[world]") {
    ashpaw::world::World world(test_map());
    const auto entity_id = world.spawn_player();

    auto opened = world.interact(entity_id, "crate_a");
    CHECK(opened.status == ashpaw::world::InteractionStatus::success);
    CHECK(opened.state_changed);
    CHECK(opened.is_open);

    auto closed = world.interact(entity_id, "crate_a");
    CHECK(closed.status == ashpaw::world::InteractionStatus::success);
    CHECK(closed.state_changed);
    CHECK_FALSE(closed.is_open);
}
