#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ashpaw::config {

enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error
};

struct ServerConfig {
    std::uint16_t listen_port {7777};
    std::uint16_t tick_rate {20};
    std::uint16_t snapshot_rate {10};
    std::uint16_t max_players {16};
    std::uint16_t visibility_radius_units {160};
    std::uint16_t max_packet_size_bytes {512};
    std::uint16_t max_display_name_length {24};
    std::uint16_t max_chat_message_length {120};
    std::filesystem::path startup_map {"maps/dev_map.json"};
    std::filesystem::path player_save_dir {"data/players"};
    LogLevel log_level {LogLevel::info};
};

struct CliOptions {
    std::filesystem::path config_path {"config/server.toml"};
    std::optional<std::uint16_t> port_override;
    std::optional<LogLevel> log_level_override;
};

[[nodiscard]] LogLevel parse_log_level(std::string_view value);
[[nodiscard]] std::string to_string(LogLevel level);
[[nodiscard]] CliOptions parse_cli_options(int argc, char** argv);
[[nodiscard]] ServerConfig load_server_config(const std::filesystem::path& path);
[[nodiscard]] ServerConfig apply_cli_overrides(ServerConfig config, const CliOptions& options);

}  // namespace ashpaw::config
