#pragma once

#include "ashpaw/config/server_config.hpp"

#include <memory>

namespace spdlog {
class logger;
}

namespace ashpaw::util {

void initialize_logging(config::LogLevel level);
std::shared_ptr<spdlog::logger> logger();

}  // namespace ashpaw::util
