#pragma once

#include "net.h"
#include "protocol.h"
#include <google/protobuf/message.h>
#include <functional>
#include <memory>


namespace net {

// 通用的消息处理器接口
class IMessageHandler {
public:
    virtual ~IMessageHandler() = default;
    virtual bool handle(const std::string& data, Buffer* response) = 0;
    virtual ProtocolType get_type() const = 0;
};

// Protobuf 消息处理器
template<typename RequestType, typename ResponseType = google::protobuf::Message>
class ProtobufMessageHandler : public IMessageHandler {
public:
    using HandlerFunc = std::function<void(const RequestType&, ResponseType&)>;
    
    ProtobufMessageHandler(HandlerFunc handler) : handler_(std::move(handler)) {}
    
    bool handle(const std::string& data, Buffer* response) override {
        RequestType request;
        if (!request.ParseFromString(data)) {
            ERR("Failed to parse protobuf message");
            return false;
        }
        
        ResponseType response_msg;
        try {
            handler_(request, response_msg);
            
            // 序列化响应
            if (response) {
                std::string serialized;
                if (response_msg.SerializeToString(&serialized)) {
                    ProtocolTools::LVProtocol protocol(ProtocolType::PROTOBUF, serialized);
                    response->append(protocol.to_string());
                }
            }
            return true;
        } catch (const std::exception& e) {
            ERR("Protobuf handler error: {}", e.what());
            return false;
        }
    }
    
    ProtocolType get_type() const override {
        return ProtocolType::PROTOBUF;
    }
    
private:
    HandlerFunc handler_;
};

// JSON 消息处理器
class JsonMessageHandler : public IMessageHandler {
public:
    using HandlerFunc = std::function<void(const Json::Value&, Json::Value&)>;
    
    JsonMessageHandler(HandlerFunc handler) : handler_(std::move(handler)) {}
    
    bool handle(const std::string& data, Buffer* response) override {
        Json::Value request;
        if (!ProtocolTools::deserialize(data.c_str(), data.size(), &request)) {
            ERR("Failed to parse JSON message");
            return false;
        }
        
        Json::Value response_msg;
        try {
            handler_(request, response_msg);
            
            // 序列化响应
            if (response) {
                std::string serialized;
                ProtocolTools::serialize(response_msg, &serialized);
                ProtocolTools::LVProtocol protocol(ProtocolType::JSON, serialized);
                response->append(protocol.to_string());
            }
            return true;
        } catch (const std::exception& e) {
            ERR("JSON handler error: {}", e.what());
            return false;
        }
    }
    
    ProtocolType get_type() const override {
        return ProtocolType::JSON;
    }
    
private:
    HandlerFunc handler_;
};

class Provider {
public:
    Provider(int port) : 
        server_(std::make_shared<Server>(ioc_, port, [this](Buffer& recv, Buffer* send) { 
            this->service(recv, send); 
        })),
        work_(ioc_)
    {}

    // 注册 Protobuf 消息处理器
    template<typename RequestType, typename ResponseType = google::protobuf::Message>
    void register_protobuf_handler(typename ProtobufMessageHandler<RequestType, ResponseType>::HandlerFunc handler) {
        auto handler_ptr = std::make_shared<ProtobufMessageHandler<RequestType, ResponseType>>(std::move(handler));
        handlers_.push_back(handler_ptr);
        INF("Registered protobuf handler for type: {}", typeid(RequestType).name());
    }

    // 注册 JSON 消息处理器
    void register_json_handler(JsonMessageHandler::HandlerFunc handler) {
        auto handler_ptr = std::make_shared<JsonMessageHandler>(std::move(handler));
        handlers_.push_back(handler_ptr);
        INF("Registered JSON handler");
    }

    void start() {
        server_->start();
        std::thread([this]() {
            ioc_.run();
        }).detach();
    }

    void stop() {
        server_->stop();
        ioc_.stop();
        INF("Provider stopped");
    }

private:
    void service(Buffer& recv, Buffer* send) {
        // 解析读取的数据
        std::string data = recv.read_all_as_string();
        INF("收到了 {} 字节的数据", data.size());

        static ProtocolTools::BufferHandler buffer_handler;
        buffer_handler.append(data.data(), data.size());
        
        ProtocolTools::LVProtocol message;
        while (buffer_handler.get_next_message(message)) {
            handle_single_message(message, send);
        }
    }

    void handle_single_message(const ProtocolTools::LVProtocol& message, Buffer* send) {
        INF("处理消息, 类型: {}, 长度: {}", 
            (message.type == ProtocolType::PROTOBUF ? "Protobuf" : "JSON"), 
            message.length);

        // 尝试所有注册的处理器
        bool handled = false;
        for (const auto& handler : handlers_) {
            if (handler->get_type() == message.type) {
                if (handler->handle(message.data, send)) {
                    handled = true;
                    break;
                }
            }
        }

        if (!handled) {
            ERR("No suitable handler found for message type: {}", 
                (message.type == ProtocolType::PROTOBUF ? "Protobuf" : "JSON"));
            // 可以发送错误响应
            send_error_response(message.type, "No handler found", send);
        }
    }

    void send_error_response(ProtocolType type, const std::string& error_msg, Buffer* send) {
        if (!send) return;

        if (type == ProtocolType::JSON) {
            Json::Value error_response;
            error_response["status"] = "error";
            error_response["message"] = error_msg;
            error_response["timestamp"] = static_cast<Json::Int64>(time(nullptr));
            
            std::string serialized;
            ProtocolTools::serialize(error_response, &serialized);
            ProtocolTools::LVProtocol protocol(ProtocolType::JSON, serialized);
            send->append(protocol.to_string());
        } else {
            // 对于 Protobuf，可以创建一个通用的错误消息类型
            // 这里简单使用 JSON 作为错误响应
            send_error_response(ProtocolType::JSON, error_msg, send);
        }
    }

private:
    asio::io_context ioc_;
    std::shared_ptr<Server> server_;
    asio::io_context::work work_;
    std::vector<std::shared_ptr<IMessageHandler>> handlers_;
};

} // namespace net

