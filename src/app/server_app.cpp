#include "ashpaw/app/server_app.hpp"

#include "ashpaw/util/logging.hpp"

#include <enet/enet.h>

#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>

namespace ashpaw::app {

namespace {

ServerApp* g_running_app = nullptr;

void handle_signal(int signal_value) {
    if (signal_value == SIGINT && g_running_app != nullptr) {
        g_running_app->request_stop();
    }
}

}  // namespace

ServerApp::ServerApp(config::ServerConfig config)
    : config_(std::move(config)),
      map_(world::load_map(config_.startup_map)),
      world_(map_),
      sessions_(config_.max_players),
      clock_(config_.tick_rate),
      snapshot_interval_(std::chrono::milliseconds(1000 / config_.snapshot_rate)),
      network_(config_, world_, sessions_) {}

void ServerApp::initialize() {
    if (enet_initialize() != 0) {
        throw std::runtime_error("failed to initialize ENet");
    }

    util::initialize_logging(config_.log_level);
    network_.initialize();
    initialized_ = true;
    g_running_app = this;
    std::signal(SIGINT, handle_signal);
    spdlog::info("server initialized on port {}", config_.listen_port);
}

int ServerApp::run() {
    if (!initialized_) {
        throw std::runtime_error("server must be initialized before run");
    }

    auto previous = std::chrono::steady_clock::now();

    while (!stop_requested_.load()) {
        const auto now = std::chrono::steady_clock::now();
        clock_.add_elapsed(now - previous);
        snapshot_accumulator_ += now - previous;
        previous = now;

        network_.service(std::chrono::milliseconds(1));

        while (clock_.should_step()) {
            world_.tick(clock_.step_seconds());
            clock_.consume_step();
        }

        while (snapshot_accumulator_ >= snapshot_interval_) {
            network_.broadcast_snapshots();
            snapshot_accumulator_ -= snapshot_interval_;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    spdlog::info("server shutdown requested");
    shutdown();
    return 0;
}

void ServerApp::shutdown() {
    network_.shutdown();
    if (initialized_) {
        enet_deinitialize();
        initialized_ = false;
    }
    if (g_running_app == this) {
        g_running_app = nullptr;
    }
}

void ServerApp::request_stop() noexcept {
    stop_requested_.store(true);
}

world::World& ServerApp::world() noexcept {
    return world_;
}

net::NetworkServer& ServerApp::network() noexcept {
    return network_;
}

}  // namespace ashpaw::app
