#include <etcd.h>
#include <net.h>
#include <test.pb.h>
#include <protocol.h>
const std::string etcd_addr = "http://127.0.0.1:2379";

void online_callback(const std::string& service_name, const std::string& endpoint)
{
    INF("{}服务上线了, endpoint={}", service_name, endpoint);
    // 发送一个Protobuf数据
    boost::asio::io_context ioc_;
    const std::string host = endpoint.substr(0, endpoint.find(':'));
    std::string port = endpoint.substr(endpoint.find(':') + 1);
    std::shared_ptr<net::Client> client = std::make_shared<net::Client>(ioc_, host, port, 
        [](net::Buffer& recv)
        {
            // 解析Protobuf数据
            std::string recv_str = recv.read_all_as_string();
            INF("收到{}字节的数据", recv_str.size());

            static ProtocolTools::BufferHandler buffer_handler;
            buffer_handler.append(recv_str.data(), recv_str.size());
            
            ProtocolTools::LVProtocol message;
            while (buffer_handler.get_next_message(message)) {
                INF("处理消息, 类型: {}, 长度: {}", 
                (message.type == ProtocolType::PROTOBUF ? "Protobuf" : "JSON"), 
                message.length);
                AddResponse response;
                response.ParseFromString(message.data);
                INF("结果: {}", response.result());
            }
        });
    client->start();
    boost::asio::io_context::work work(ioc_);
    std::thread([&ioc_]() { ioc_.run(); }).detach();
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // 发送数据

    AddRequest request;
    request.set_a(1);
    request.set_b(2);
    std::string packed_data;
    ProtocolTools::pack_protobuf(request, &packed_data);
    client->send(packed_data);

    std::this_thread::sleep_for(std::chrono::seconds(10));
}

void offline_callback(const std::string& service_name, const std::string& endpoint)
{
    INF("{}服务下线了, endpoint={}", service_name, endpoint);
}



void test_etcd_discovery()
{   
    // 创建一个服务的发现者
    auto discovery = std::make_shared<Tools::ServiceDiscovery>(etcd_addr, online_callback, offline_callback);

    discovery->watch_service();

    INF("等待服务上线...");
    getchar();
}
int main()
{
    init_global_logging();
    test_etcd_discovery();
    return 0;
}
