#pragma once

#include "ashpaw/config/server_config.hpp"
#include "ashpaw/net/network_server.hpp"
#include "ashpaw/session/session_manager.hpp"
#include "ashpaw/simulation/simulation_clock.hpp"
#include "ashpaw/world/world.hpp"

#include <atomic>
#include <chrono>
#include <memory>

namespace ashpaw::app {

class ServerApp {
  public:
    explicit ServerApp(config::ServerConfig config);

    void initialize();
    int run();
    void shutdown();
    void request_stop() noexcept;

    [[nodiscard]] world::World& world() noexcept;
    [[nodiscard]] net::NetworkServer& network() noexcept;

  private:
    config::ServerConfig config_;
    world::MapData map_;
    world::World world_;
    session::SessionManager sessions_;
    simulation::SimulationClock clock_;
    std::chrono::steady_clock::duration snapshot_accumulator_ {};
    std::chrono::steady_clock::duration snapshot_interval_ {};
    net::NetworkServer network_;
    std::atomic_bool stop_requested_ {false};
    bool initialized_ {false};
};

}  // namespace ashpaw::app
