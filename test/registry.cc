
#include <etcd.h>
#include <server.h>
#include "test.pb.h"

const std::string etcd_addr = "http://127.0.0.1:2379";

void test_registry() {
    std::shared_ptr<Tools::ServiceProvider> provider
        = std::make_shared<Tools::ServiceProvider>(etcd_addr);
    // 注册服务
    provider->register_service("127.0.0.1:8080", "Add");

    std::shared_ptr<net::Provider> net_provider = std::make_shared<net::Provider>(8080);

    
    net_provider->register_protobuf_handler<AddRequest, AddResponse>(
    [](const AddRequest& req, AddResponse& rsp) {
        rsp.set_result(req.a() + req.b());
    });

    net_provider->start();

    INF("Press any key to exit");
    getchar();

    net_provider->stop();

    provider->deregister_service();
}

int main()
{
    init_global_logging();
    test_registry();
    return 0;
}