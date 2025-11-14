#include "../include/log.h"

namespace Tools
{
    std::shared_ptr<spdlog::logger> g_logger;

    void init_log(const log_config& config)
    {
        // 判断同步还是异步
        if(config.async_)
        {
            if(config.target_ == "stdout" || config.target_ == "console")
                g_logger = spdlog::stdout_color_mt<spdlog::async_factory>("logger");
            else
                g_logger = spdlog::basic_logger_mt<spdlog::async_factory>("logger", config.target_);
        }
        else
        {
            if(config.target_ == "stdout" || config.target_ == "console")
                g_logger = spdlog::stdout_color_mt("logger");
            else
                g_logger = spdlog::basic_logger_mt("logger", config.target_);
        }
        // 日志等级
        g_logger->set_level(static_cast<spdlog::level::level_enum>(config.level_));
        // 日志格式
        g_logger->set_pattern(config.pattern_);
    }
}