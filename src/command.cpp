#include "../include/command.h"

DEFINE_bool(log_async, true, "是否启用异步日志");
DEFINE_string(log_target, "stdout", "日志输出目标,可选值:stdout、filem、console");
DEFINE_int32(log_level, 1, "日志级别,0-trace, 1-debug, 2-info, 3-warn, 4-err");
DEFINE_string(log_pattern, "[%H:%M:%S][%-7l]%v", "日志的输出格式");

void init_global_logging()
{
    // 定义日志配置信息
    Tools::log_config config = {
        .async_ = FLAGS_log_async,
        .level_ = FLAGS_log_level,
        .pattern_ = FLAGS_log_pattern,
        .target_ = FLAGS_log_target
    };

    // 初始化日志器
    Tools::init_log(config);
}