#include "ashpaw/net/protocol.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("client hello round-trips through header decode", "[protocol]") {
    const auto encoded = ashpaw::net::encode_client_hello(ashpaw::net::ClientHello {
        .protocol_version = ashpaw::net::kProtocolVersion,
        .display_name = "Marten"
    });

    const auto incoming = ashpaw::net::decode_header(encoded);
    REQUIRE(incoming.has_value());
    CHECK(incoming->opcode == ashpaw::net::Opcode::client_hello);

    const auto hello = ashpaw::net::decode_client_hello(incoming->payload);
    REQUIRE(hello.has_value());
    CHECK(hello->protocol_version == ashpaw::net::kProtocolVersion);
    CHECK(hello->display_name == "Marten");
}

TEST_CASE("movement input is clamped during decode", "[protocol]") {
    const std::vector<std::uint8_t> encoded {
        static_cast<std::uint8_t>(ashpaw::net::Opcode::movement_input),
        static_cast<std::uint8_t>(5),
        static_cast<std::uint8_t>(-4)
    };

    const auto incoming = ashpaw::net::decode_header(encoded);
    REQUIRE(incoming.has_value());

    const auto movement = ashpaw::net::decode_movement_input(incoming->payload);
    REQUIRE(movement.has_value());
    CHECK(movement->move_x == 1);
    CHECK(movement->move_y == -1);
}

TEST_CASE("invalid opcode is rejected", "[protocol]") {
    const std::vector<std::uint8_t> encoded {99, 1, 2, 3};
    CHECK_FALSE(ashpaw::net::decode_header(encoded).has_value());
}

TEST_CASE("interaction request round-trips through header decode", "[protocol]") {
    const auto encoded = ashpaw::net::encode_interaction_request(ashpaw::net::InteractionRequest {
        .target_id = "town_sign"
    });

    const auto incoming = ashpaw::net::decode_header(encoded);
    REQUIRE(incoming.has_value());
    CHECK(incoming->opcode == ashpaw::net::Opcode::interaction_request);

    const auto request = ashpaw::net::decode_interaction_request(incoming->payload);
    REQUIRE(request.has_value());
    CHECK(request->target_id == "town_sign");
}

TEST_CASE("chat send round-trips through header decode", "[protocol]") {
    const auto encoded = ashpaw::net::encode_chat_send(ashpaw::net::ChatSend {
        .message = "Hello meadow"
    });

    const auto incoming = ashpaw::net::decode_header(encoded);
    REQUIRE(incoming.has_value());
    CHECK(incoming->opcode == ashpaw::net::Opcode::chat_send);

    const auto chat = ashpaw::net::decode_chat_send(incoming->payload);
    REQUIRE(chat.has_value());
    CHECK(chat->message == "Hello meadow");
}
