#include "logger.hpp"

#include <memory>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace LunarCore {

void Logger::init() {
    spdlog::init_thread_pool(8192, 1);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "lunar", 
        console_sink, 
        spdlog::thread_pool(), 
        spdlog::async_overflow_policy::block
    );

    logger->set_pattern("\033[38;2;1;1;128m[lunar]\033[0m %^[%Y-%m-%d %H:%M:%S] [%l]:%$ %v");

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
}

} // namespace LunarCore