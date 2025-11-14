#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <string>

namespace Tools
{
    // 日志的参数配置
    struct log_config
    {
        bool async_;
        int level_;
        std::string pattern_;
        std::string target_;
    };
    
    // 声明全局日志器
    extern std::shared_ptr<spdlog::logger> g_logger;

    // 全局日志器初始化接口
    void init_log(const log_config& config);

    constexpr std::string_view default_prefix =  "[{}:{}] ";
    // 封装全局的日志输出宏
    #define DBG(fmt, ...) Tools::g_logger->debug("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define INF(fmt, ...) Tools::g_logger->info("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define WAR(fmt, ...) Tools::g_logger->warn("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define ERR(fmt, ...) Tools::g_logger->error("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)


    
}