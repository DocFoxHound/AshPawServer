#include "ashpaw/config/server_config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

std::filesystem::path write_temp_file(const std::string& name, const std::string& contents) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path);
    output << contents;
    return path;
}

}  // namespace

TEST_CASE("server config loads expected fields", "[config]") {
    const auto path = write_temp_file("ashpaw_config_valid.toml",
                                      "listen_port = 7777\n"
                                      "tick_rate = 20\n"
                                      "snapshot_rate = 10\n"
                                      "max_players = 8\n"
                                      "visibility_radius_units = 192\n"
                                      "startup_map = \"maps/dev_map.json\"\n"
                                      "player_save_dir = \"data/players\"\n"
                                      "log_level = \"debug\"\n");

    const auto config = ashpaw::config::load_server_config(path);

    CHECK(config.listen_port == 7777);
    CHECK(config.tick_rate == 20);
    CHECK(config.snapshot_rate == 10);
    CHECK(config.max_players == 8);
    CHECK(config.visibility_radius_units == 192);
    CHECK(config.startup_map == "maps/dev_map.json");
    CHECK(config.player_save_dir == "data/players");
    CHECK(config.log_level == ashpaw::config::LogLevel::debug);
}

TEST_CASE("server config rejects missing required field", "[config]") {
    const auto path = write_temp_file("ashpaw_config_missing.toml",
                                      "listen_port = 7777\n"
                                      "tick_rate = 20\n"
                                      "max_players = 8\n"
                                      "log_level = \"info\"\n");

    CHECK_THROWS_AS(ashpaw::config::load_server_config(path), std::runtime_error);
}

TEST_CASE("cli overrides replace config values", "[config]") {
    ashpaw::config::ServerConfig config;
    ashpaw::config::CliOptions options;
    options.port_override = 9001;
    options.log_level_override = ashpaw::config::LogLevel::warn;

    const auto updated = ashpaw::config::apply_cli_overrides(config, options);

    CHECK(updated.listen_port == 9001);
    CHECK(updated.log_level == ashpaw::config::LogLevel::warn);
}
