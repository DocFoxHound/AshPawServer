#include "ashpaw/app/server_app.hpp"

#include "ashpaw/world/map_validation.hpp"
#include "ashpaw/util/logging.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/select.h>
#include <unistd.h>
#endif

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
    const auto issues = world::validate_map(map_);
    for (const auto& issue : issues) {
        spdlog::warn("map validation: {}", issue.message);
    }
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
            const auto tick_start = std::chrono::steady_clock::now();
            world_.tick(clock_.step_seconds());
            const auto tick_duration = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tick_start).count();
            runtime_metrics_.last_tick_ms = tick_duration;
            runtime_metrics_.max_tick_ms = std::max(runtime_metrics_.max_tick_ms, tick_duration);
            ++runtime_metrics_.simulation_steps;
            clock_.consume_step();
        }

        while (snapshot_accumulator_ >= snapshot_interval_) {
            network_.broadcast_snapshots();
            ++runtime_metrics_.snapshot_broadcasts;
            snapshot_accumulator_ -= snapshot_interval_;
        }

        poll_console_commands();
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

const RuntimeMetrics& ServerApp::runtime_metrics() const noexcept {
    return runtime_metrics_;
}

AdminCommandResult ServerApp::execute_admin_command(std::string_view command_line) {
    AdminCommandProcessor processor(world_, sessions_, network_, runtime_metrics_, [this]() { request_stop(); });
    const auto result = processor.execute(command_line);
    if (result.recognized) {
        if (result.success) {
            spdlog::info("admin command '{}' succeeded: {}", command_line, result.message);
        } else {
            spdlog::warn("admin command '{}' failed: {}", command_line, result.message);
        }
    }
    return result;
}

void ServerApp::poll_console_commands() {
#if defined(__unix__) || defined(__APPLE__)
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    const auto ready = select(STDIN_FILENO + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ready <= 0 || !FD_ISSET(STDIN_FILENO, &read_fds)) {
        return;
    }

    std::string command_line;
    if (!std::getline(std::cin, command_line)) {
        return;
    }

    const auto result = execute_admin_command(command_line);
    if (result.recognized && !result.message.empty()) {
        std::cout << result.message << '\n';
    }
#endif
}

}  // namespace ashpaw::app
