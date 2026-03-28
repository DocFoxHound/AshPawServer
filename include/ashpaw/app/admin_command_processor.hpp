#pragma once

#include "ashpaw/net/network_server.hpp"
#include "ashpaw/session/session_manager.hpp"
#include "ashpaw/world/world.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace ashpaw::app {

struct RuntimeMetrics {
    std::size_t simulation_steps {0};
    double last_tick_ms {0.0};
    double max_tick_ms {0.0};
    std::size_t snapshot_broadcasts {0};
};

struct AdminCommandResult {
    bool recognized {false};
    bool success {false};
    bool stop_requested {false};
    std::string message;
};

class AdminCommandProcessor {
  public:
    using StopCallback = std::function<void()>;

    AdminCommandProcessor(world::World& world,
                          session::SessionManager& sessions,
                          net::NetworkServer& network,
                          const RuntimeMetrics& runtime_metrics,
                          StopCallback stop_callback);

    [[nodiscard]] AdminCommandResult execute(std::string_view command_line);

  private:
    [[nodiscard]] session::Session* find_session_target(std::string_view token) const;

    world::World& world_;
    session::SessionManager& sessions_;
    net::NetworkServer& network_;
    const RuntimeMetrics& runtime_metrics_;
    StopCallback stop_callback_;
};

}  // namespace ashpaw::app
