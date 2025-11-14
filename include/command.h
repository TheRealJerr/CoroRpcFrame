#pragma once
#include "log.h"
#include <gflags/gflags.h>



// 定义命令行参数的处理
// 初始化全局日志器的相关的参数
// 日志级别、日志格式、日志输出目标、是否异步输出等
// 声明
// 在头文件中使用 DECLARE_ 来声明全局标志变量
DECLARE_bool(log_async);
DECLARE_string(log_target);
DECLARE_int32(log_level);
DECLARE_string(log_pattern);


void init_global_logging();
