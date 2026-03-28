#include "ashpaw/app/admin_command_processor.hpp"

#include "ashpaw/world/map_validation.hpp"

#include <cstdlib>
#include <charconv>
#include <sstream>
#include <system_error>
#include <utility>

namespace ashpaw::app {

namespace {

std::string trim_copy(std::string_view value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(start, end - start + 1));
}

bool parse_u32(std::string_view token, std::uint32_t& value) {
    const auto* begin = token.data();
    const auto* end = token.data() + token.size();
    return std::from_chars(begin, end, value).ec == std::errc {};
}

bool parse_float(std::string_view token, float& value) {
    std::string owned(token);
    char* parse_end = nullptr;
    value = std::strtof(owned.c_str(), &parse_end);
    return parse_end != nullptr && *parse_end == '\0';
}

std::string format_vec2(ashpaw::world::Vec2 value) {
    std::ostringstream output;
    output << "(" << value.x << ", " << value.y << ")";
    return output.str();
}

}  // namespace

AdminCommandProcessor::AdminCommandProcessor(world::World& world,
                                             session::SessionManager& sessions,
                                             net::NetworkServer& network,
                                             const RuntimeMetrics& runtime_metrics,
                                             StopCallback stop_callback)
    : world_(world),
      sessions_(sessions),
      network_(network),
      runtime_metrics_(runtime_metrics),
      stop_callback_(std::move(stop_callback)) {}

AdminCommandResult AdminCommandProcessor::execute(std::string_view command_line) {
    std::istringstream input(trim_copy(command_line));
    std::string command;
    input >> command;

    if (command.empty()) {
        return {
            .recognized = false,
            .success = false,
            .message = "empty command"
        };
    }

    if (command == "help") {
        return {
            .recognized = true,
            .success = true,
            .message = "commands: help, players, kick <name|entity_id>, teleport <name|entity_id> <x> <y>, inspect <entity_id|interactable_id>, metrics, validate_map, stop"
        };
    }

    if (command == "players") {
        auto active_sessions = sessions_.active_sessions();
        std::ostringstream output;
        output << active_sessions.size() << " active player(s)";
        for (auto* session : active_sessions) {
            const auto entity = world_.entity(session->entity_id);
            output << "\n- session=" << session->session_id
                   << " entity=" << session->entity_id
                   << " name=" << session->display_name;
            if (entity.has_value()) {
                output << " position=" << format_vec2(entity->position);
            }
        }
        return {
            .recognized = true,
            .success = true,
            .message = output.str()
        };
    }

    if (command == "kick") {
        std::string target;
        input >> target;
        if (target.empty()) {
            return {.recognized = true, .success = false, .message = "usage: kick <name|entity_id>"};
        }

        auto* session = find_session_target(target);
        if (session == nullptr) {
            return {.recognized = true, .success = false, .message = "player not found"};
        }

        const auto success = network_.disconnect_session(*session, "admin kick");
        return {
            .recognized = true,
            .success = success,
            .message = success ? "kick requested for " + session->display_name : "failed to disconnect player"
        };
    }

    if (command == "teleport") {
        std::string target;
        std::string x_token;
        std::string y_token;
        input >> target >> x_token >> y_token;
        if (target.empty() || x_token.empty() || y_token.empty()) {
            return {.recognized = true, .success = false, .message = "usage: teleport <name|entity_id> <x> <y>"};
        }

        auto* session = find_session_target(target);
        if (session == nullptr) {
            return {.recognized = true, .success = false, .message = "player not found"};
        }

        float x = 0.0F;
        float y = 0.0F;
        if (!parse_float(x_token, x) || !parse_float(y_token, y)) {
            return {.recognized = true, .success = false, .message = "invalid coordinates"};
        }

        const auto success = world_.set_entity_position(session->entity_id, {.x = x, .y = y});
        return {
            .recognized = true,
            .success = success,
            .message = success ? "teleported " + session->display_name + " to " + format_vec2({.x = x, .y = y})
                              : "teleport target position is blocked or invalid"
        };
    }

    if (command == "inspect") {
        std::string target;
        input >> target;
        if (target.empty()) {
            return {.recognized = true, .success = false, .message = "usage: inspect <entity_id|interactable_id>"};
        }

        std::uint32_t entity_id = 0;
        if (parse_u32(target, entity_id)) {
            const auto entity = world_.entity(entity_id);
            if (!entity.has_value()) {
                return {.recognized = true, .success = false, .message = "entity not found"};
            }

            std::ostringstream output;
            output << "entity=" << entity->id
                   << " position=" << format_vec2(entity->position)
                   << " movement=(" << static_cast<int>(entity->movement_intent.x)
                   << ", " << static_cast<int>(entity->movement_intent.y) << ")";
            return {.recognized = true, .success = true, .message = output.str()};
        }

        const auto object = world_.interactable(target);
        if (!object.has_value()) {
            return {.recognized = true, .success = false, .message = "target not found"};
        }

        std::ostringstream output;
        output << "interactable=" << object->id
               << " type=" << static_cast<int>(object->type)
               << " position=" << format_vec2(object->position)
               << " open=" << (object->is_open ? "true" : "false");
        if (object->occupant_entity_id.has_value()) {
            output << " occupant=" << *object->occupant_entity_id;
        }
        if (!object->text.empty()) {
            output << " text=\"" << object->text << "\"";
        }
        return {.recognized = true, .success = true, .message = output.str()};
    }

    if (command == "metrics") {
        const auto& network_metrics = network_.metrics();
        std::ostringstream output;
        output << "active_players=" << sessions_.active_sessions().size()
               << " entities=" << world_.entity_count()
               << " interactables=" << world_.interactable_count()
               << " steps=" << runtime_metrics_.simulation_steps
               << " last_tick_ms=" << runtime_metrics_.last_tick_ms
               << " max_tick_ms=" << runtime_metrics_.max_tick_ms
               << " snapshot_broadcasts=" << runtime_metrics_.snapshot_broadcasts
               << " snapshot_packets=" << network_metrics.snapshot_packets_sent
               << " snapshot_entries=" << network_metrics.snapshot_entries_sent
               << " object_state_packets=" << network_metrics.object_state_packets_sent
               << " object_state_suppressed=" << network_metrics.object_state_packets_suppressed;
        return {.recognized = true, .success = true, .message = output.str()};
    }

    if (command == "validate_map") {
        const auto issues = world::validate_map(world_.map());
        if (issues.empty()) {
            return {.recognized = true, .success = true, .message = "map validation passed"};
        }

        std::ostringstream output;
        output << issues.size() << " map validation issue(s)";
        for (const auto& issue : issues) {
            output << "\n- " << issue.message;
        }
        return {.recognized = true, .success = false, .message = output.str()};
    }

    if (command == "stop") {
        if (stop_callback_) {
            stop_callback_();
        }
        return {.recognized = true, .success = true, .stop_requested = true, .message = "server stop requested"};
    }

    return {
        .recognized = false,
        .success = false,
        .message = "unknown command"
    };
}

session::Session* AdminCommandProcessor::find_session_target(std::string_view token) const {
    std::uint32_t entity_id = 0;
    if (parse_u32(token, entity_id)) {
        return sessions_.find_by_entity_id(entity_id);
    }

    return sessions_.find_by_display_name(token);
}

}  // namespace ashpaw::app
