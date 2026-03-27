#include "ashpaw/util/logging.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace ashpaw::util {

namespace {

spdlog::level::level_enum to_spdlog_level(config::LogLevel level) {
    switch (level) {
        case config::LogLevel::trace:
            return spdlog::level::trace;
        case config::LogLevel::debug:
            return spdlog::level::debug;
        case config::LogLevel::info:
            return spdlog::level::info;
        case config::LogLevel::warn:
            return spdlog::level::warn;
        case config::LogLevel::error:
            return spdlog::level::err;
    }
    return spdlog::level::info;
}

}  // namespace

void initialize_logging(config::LogLevel level) {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto shared_logger = std::make_shared<spdlog::logger>("ashpaw", std::move(sink));
    shared_logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    shared_logger->set_level(to_spdlog_level(level));
    spdlog::set_default_logger(shared_logger);
    spdlog::set_level(to_spdlog_level(level));
}

std::shared_ptr<spdlog::logger> logger() {
    return spdlog::default_logger();
}

}  // namespace ashpaw::util
