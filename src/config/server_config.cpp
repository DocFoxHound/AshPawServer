#include "ashpaw/config/server_config.hpp"

#include <limits>
#include <stdexcept>
#include <string_view>

#include <toml++/toml.h>

namespace ashpaw::config {

namespace {

[[nodiscard]] std::uint16_t require_u16(const toml::table& table, std::string_view key) {
    const auto value = table[key].value<int64_t>();
    if (!value.has_value() || *value < 1 || *value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("invalid or missing numeric config field: " + std::string(key));
    }
    return static_cast<std::uint16_t>(*value);
}

[[nodiscard]] std::string require_string(const toml::table& table, std::string_view key) {
    const auto value = table[key].value<std::string>();
    if (!value.has_value() || value->empty()) {
        throw std::runtime_error("invalid or missing string config field: " + std::string(key));
    }
    return *value;
}

}  // namespace

LogLevel parse_log_level(std::string_view value) {
    if (value == "trace") {
        return LogLevel::trace;
    }
    if (value == "debug") {
        return LogLevel::debug;
    }
    if (value == "info") {
        return LogLevel::info;
    }
    if (value == "warn") {
        return LogLevel::warn;
    }
    if (value == "error") {
        return LogLevel::error;
    }
    throw std::runtime_error("unsupported log level");
}

std::string to_string(LogLevel level) {
    switch (level) {
        case LogLevel::trace:
            return "trace";
        case LogLevel::debug:
            return "debug";
        case LogLevel::info:
            return "info";
        case LogLevel::warn:
            return "warn";
        case LogLevel::error:
            return "error";
    }
    return "info";
}

CliOptions parse_cli_options(int argc, char** argv) {
    CliOptions options {};

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg {argv[index]};

        if (arg == "--config" && index + 1 < argc) {
            options.config_path = argv[++index];
            continue;
        }

        if (arg == "--port" && index + 1 < argc) {
            const auto parsed = std::stoi(argv[++index]);
            if (parsed < 1 || parsed > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("port override out of range");
            }
            options.port_override = static_cast<std::uint16_t>(parsed);
            continue;
        }

        if (arg == "--log-level" && index + 1 < argc) {
            options.log_level_override = parse_log_level(argv[++index]);
            continue;
        }
    }

    return options;
}

ServerConfig load_server_config(const std::filesystem::path& path) {
    const toml::table table = toml::parse_file(path.string());

    ServerConfig config;
    config.listen_port = require_u16(table, "listen_port");
    config.tick_rate = require_u16(table, "tick_rate");
    config.snapshot_rate = require_u16(table, "snapshot_rate");
    config.max_players = require_u16(table, "max_players");
    config.visibility_radius_units = require_u16(table, "visibility_radius_units");
    config.startup_map = require_string(table, "startup_map");
    config.player_save_dir = require_string(table, "player_save_dir");
    config.log_level = parse_log_level(require_string(table, "log_level"));
    return config;
}

ServerConfig apply_cli_overrides(ServerConfig config, const CliOptions& options) {
    if (options.port_override.has_value()) {
        config.listen_port = *options.port_override;
    }
    if (options.log_level_override.has_value()) {
        config.log_level = *options.log_level_override;
    }
    return config;
}

}  // namespace ashpaw::config
