#include "../include/protocol.h"

#include <sstream>
#include <iostream>

// 序列化 JSON
bool ProtocolTools::serialize(const Json::Value& val, std::string* data) {
    if (!data) {
        return false;
    }
    
    try {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = ""; // 紧凑格式，无缩进
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        
        std::ostringstream oss;
        writer->write(val, &oss);
        *data = oss.str();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "JSON serialization failed: " << e.what() << std::endl;
        return false;
    }
}

// 序列化 Protobuf
bool ProtocolTools::serialize(const google::protobuf::Message& msg, std::string* data) {
    if (!data) {
        return false;
    }
    
    try {
        return msg.SerializeToString(data);
    } catch (const std::exception& e) {
        std::cerr << "Protobuf serialization failed: " << e.what() << std::endl;
        return false;
    }
}

// 反序列化 JSON
bool ProtocolTools::deserialize(const char* data, size_t len, Json::Value* val) {
    if (!data || !val || len == 0) {
        return false;
    }
    
    try {
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        
        std::string json_str(data, len);
        std::string errors;
        
        return reader->parse(json_str.c_str(), json_str.c_str() + json_str.size(), val, &errors);
    } catch (const std::exception& e) {
        std::cerr << "JSON deserialization failed: " << e.what() << std::endl;
        return false;
    }
}

// 反序列化 Protobuf
bool ProtocolTools::deserialize(const char* data, size_t len, google::protobuf::Message* msg) {
    if (!data || !msg || len == 0) {
        return false;
    }
    
    try {
        return msg->ParseFromArray(data, len);
    } catch (const std::exception& e) {
        std::cerr << "Protobuf deserialization failed: " << e.what() << std::endl;
        return false;
    }
}

// 打包 Protobuf 消息
bool ProtocolTools::pack_protobuf(const google::protobuf::Message& msg, std::string* packed_data) {
    if (!packed_data) {
        return false;
    }
    
    std::string serialized_data;
    if (!serialize(msg, &serialized_data)) {
        return false;
    }
    
    LVProtocol protocol(ProtocolType::PROTOBUF, serialized_data);
    *packed_data = protocol.to_string();
    return true;
}

// 打包 JSON 消息
bool ProtocolTools::pack_json(const Json::Value& json, std::string* packed_data) {
    if (!packed_data) {
        return false;
    }
    
    std::string serialized_data;
    if (!serialize(json, &serialized_data)) {
        return false;
    }
    
    LVProtocol protocol(ProtocolType::JSON, serialized_data);
    *packed_data = protocol.to_string();
    return true;
}

// 打包协议
bool ProtocolTools::pack(const LVProtocol& protocol, std::string* packed_data) {
    if (!packed_data) {
        return false;
    }
    
    *packed_data = protocol.to_string();
    return true;
}

// 解析消息头
bool ProtocolTools::parse_message_header(const std::string& data, size_t& data_start, int& length, ProtocolType& type) {
    size_t pos = 0;
    const std::string gap = "\r\n";
    
    // 查找长度字段结束位置
    size_t len_end = data.find(gap, pos);
    if (len_end == std::string::npos) {
        return false;
    }
    
    // 解析长度
    std::string len_str = data.substr(pos, len_end - pos);
    try {
        length = std::stoi(len_str);
    } catch (const std::exception&) {
        return false;
    }
    
    // 移动到类型字段
    pos = len_end + gap.size();
    size_t type_end = data.find(gap, pos);
    if (type_end == std::string::npos) {
        return false;
    }
    
    // 解析类型
    std::string type_str = data.substr(pos, type_end - pos);
    if (type_str == "PB") {
        type = ProtocolType::PROTOBUF;
    } else if (type_str == "JS") {
        type = ProtocolType::JSON;
    } else {
        return false;
    }
    
    // 计算数据开始位置
    data_start = type_end + gap.size();
    return true;
}

// 查找完整消息的结束位置
bool ProtocolTools::find_complete_message(const std::string& data, size_t& message_end) {
    size_t data_start;
    int length;
    ProtocolType type;
    
    if (!parse_message_header(data, data_start, length, type)) {
        return false;
    }
    
    // 检查数据长度是否足够
    const std::string gap = "\r\n";
    size_t expected_end = data_start + length + gap.size();
    
    if (expected_end > data.size()) {
        return false; // 数据不完整
    }
    
    // 验证结束分隔符
    if (data.substr(data_start + length, gap.size()) != gap) {
        return false; // 格式错误
    }
    
    message_end = expected_end;
    return true;
}

// 解包数据（处理粘包）
bool ProtocolTools::unpack(const char* data, size_t len, std::vector<LVProtocol>& messages) {
    return unpack(std::string(data, len), messages);
}

bool ProtocolTools::unpack(const std::string& data, std::vector<LVProtocol>& messages) {
    messages.clear();
    size_t pos = 0;
    
    while (pos < data.size()) {
        size_t message_end;
        
        if (!find_complete_message(data.substr(pos), message_end)) {
            break; // 没有找到完整消息
        }
        
        // 提取完整消息
        std::string complete_message = data.substr(pos, message_end);
        LVProtocol protocol;
        
        if (protocol.from_string(complete_message)) {
            messages.push_back(protocol);
        }
        
        pos += message_end;
    }
    
    return !messages.empty();
}

// BufferHandler 实现
void ProtocolTools::BufferHandler::append(const char* data, size_t len) {
    buffer_.append(data, len);
}

void ProtocolTools::BufferHandler::append(const std::string& data) {
    buffer_.append(data);
}

bool ProtocolTools::BufferHandler::get_next_message(LVProtocol& message) {
    size_t message_end;
    
    if (!find_complete_message(buffer_, message_end)) {
        return false; // 没有完整消息
    }
    
    // 提取并解析消息
    std::string complete_message = buffer_.substr(0, message_end);
    if (!message.from_string(complete_message)) {
        return false;
    }
    
    // 从缓冲区移除已处理的消息
    buffer_.erase(0, message_end);
    return true;
}