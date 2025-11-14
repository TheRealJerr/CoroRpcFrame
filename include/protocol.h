// protocol_tools.h
#pragma once
#include <google/protobuf/message.h>
#include <jsoncpp/json/json.h>
#include <string>
#include <vector>
#include <memory>

// 支持协议的消息类型
enum class ProtocolType
{
    PROTOBUF,
    JSON,
};

class ProtocolTools
{
public:
    // LV协议结构：length + gap + data
    struct LVProtocol
    {
        ProtocolType type;
        int length;
        std::string data;
        std::string gap;
        
        // 默认构造函数
        LVProtocol() : type(ProtocolType::PROTOBUF), length(0), gap("\r\n") {}
        
        // 带参构造函数
        LVProtocol(ProtocolType t, const std::string& d, const std::string& g = "\r\n")
            : type(t), data(d), gap(g) {
            length = data.size();
        }
        
        // 转换为完整的协议字符串
        std::string to_string() const {
            std::string result;
            
            // 添加长度字段
            result += std::to_string(length);
            result += gap;
            
            // 添加数据类型标识
            if (type == ProtocolType::PROTOBUF) {
                result += "PB";
            } else {
                result += "JS";
            }
            result += gap;
            
            // 添加数据
            result += data;
            result += gap;
            
            return result;
        }
        
        // 从字符串解析
        bool from_string(const std::string& str) {
            size_t pos = 0;
            
            // 解析长度字段
            size_t len_end = str.find(gap, pos);
            if (len_end == std::string::npos) {
                return false;
            }
            
            std::string len_str = str.substr(pos, len_end - pos);
            try {
                length = std::stoi(len_str);
            } catch (const std::exception&) {
                return false;
            }
            
            // 移动到类型字段
            pos = len_end + gap.size();
            size_t type_end = str.find(gap, pos);
            if (type_end == std::string::npos) {
                return false;
            }
            
            std::string type_str = str.substr(pos, type_end - pos);
            if (type_str == "PB") {
                type = ProtocolType::PROTOBUF;
            } else if (type_str == "JS") {
                type = ProtocolType::JSON;
            } else {
                return false;
            }
            
            // 解析数据字段
            pos = type_end + gap.size();
            if (pos + length + gap.size() > str.size()) {
                return false;
            }
            
            data = str.substr(pos, length);
            
            // 验证结束分隔符
            if (str.substr(pos + length, gap.size()) != gap) {
                return false;
            }
            
            return true;
        }
    };

public:
    // 序列化接口
    static bool serialize(const Json::Value& val, std::string* data);
    static bool serialize(const google::protobuf::Message& msg, std::string* data);
    
    // 反序列化接口
    static bool deserialize(const char* data, size_t len, Json::Value* val);
    static bool deserialize(const char* data, size_t len, google::protobuf::Message* msg);
    
    // 协议打包接口
    static bool pack_protobuf(const google::protobuf::Message& msg, std::string* packed_data);
    static bool pack_json(const Json::Value& json, std::string* packed_data);
    static bool pack(const LVProtocol& protocol, std::string* packed_data);
    
    // 协议解析接口（解决粘包问题）
    static bool unpack(const char* data, size_t len, std::vector<LVProtocol>& messages);
    static bool unpack(const std::string& data, std::vector<LVProtocol>& messages);
    
    // 缓冲区处理（用于处理不完整的数据包）
    class BufferHandler {
    public:
        BufferHandler() : buffer_("") {}
        
        // 添加新数据并尝试解析完整消息
        void append(const char* data, size_t len);
        void append(const std::string& data);
        
        // 获取解析出的完整消息
        bool get_next_message(LVProtocol& message);
        
        // 获取剩余缓冲区大小
        size_t remaining_size() const { return buffer_.size(); }
        
        // 清空缓冲区
        void clear() { buffer_.clear(); }
        
    private:
        std::string buffer_;
    };

private:
    // 内部辅助方法
    static bool find_complete_message(const std::string& data, size_t& message_end);
    static bool parse_message_header(const std::string& data, size_t& data_start, int& length, ProtocolType& type);
};