#pragma once
// 实现对于jsoncpp的封装
#include <jsoncpp/json/json.h>
#include <fstream>
#include "log.h"
#include <regex>
#include <optional>
#include <vector>
#include <random>
#include <iomanip>

#define DEFAULT_UUID_LEN 16
#define DAFAULT_LEASE_TIME 3
// 统一包含在命名空间
namespace Tools
{
    // json工具类
    class JsonTools
    {
    public:
        // 序列化Json::Value对象为字符串
        static std::optional<std::string> serialize(const Json::Value& value);

        // 反序列化字符串为Json::Value对象
        static std::optional<Json::Value> deserialize(const std::string& str);

    };
    // 文件工具类
    class FileTools
    {
    public:
        static bool read_from_file(const std::string& file_path, std::string* content);
        /*
            将content写入到file_path文件中
            如果文件不存在，则创建文件
            如果文件存在，则覆盖文件
        */
        static bool write_to_file(const std::string& file_path, const std::string& content);
    };

    class StringTools
    {
    public:
        // 字符串分割
        static std::vector<std::string> split(const std::string& str, 
            const std::string& sep);
        // 正则匹配
        static bool regex_match(const std::string& str, const std::string& pattern);
    };

    // 随机字符串生成
    enum class RandomType
    {
        ALPHA,
        NUM,
        MIX,
    };
    class RandomTools
    {
    public:
        // 固定length长度字母 + 数字的随机字符串
        // @param : length 字符串长度 , with_alpha 是否包含字母
        static std::string generate_random_string(int length = DEFAULT_UUID_LEN, 
            RandomType type = RandomType::MIX);
    };
}