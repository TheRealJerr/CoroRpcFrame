#include <etcd.h>

namespace Tools
{
    void default_error_handler(std::exception_ptr eptr) {
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } catch(const std::runtime_error& e) {
            ERR("连接失败: {}", e.what());
            std::cerr << "Connection failure \"" << e.what() << "\"\n";
        } catch(const std::out_of_range& e) {
            ERR("Lease expiry: {}", e.what());
            std::cerr << "Lease expiry \"" << e.what() << "\"\n";
        }
    }

    // 等待连接成功
    void wait_for_connection(etcd::Client& etcd_cli)
    {
        // INF("等待连接...");
        while(etcd_cli.head().get().is_ok() == false)
        {
            WAR("连接失败,等待1秒...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            INF("重新连接...");
        }
    }

    ServiceProvider::ServiceProvider(const std::string& rigister_addr) : 
            rigister_addr_(rigister_addr),
            node_id_(RandomTools::generate_random_string())
    {}
        
    // 注册服务
    bool ServiceProvider::register_service(const std::string& node_addr, 
        const std::string& method)
    {
        service_name_ = method;
        node_addr_ = node_addr;

        etcd::Client etcd_cli(rigister_addr_);
        // 这里可以设置连接次数, 超时时间, 重试次数等
        wait_for_connection(etcd_cli);
        // 实例化租约对象
        auto rsp = etcd_cli.leasegrant(DAFAULT_LEASE_TIME).get();
        if(rsp.is_ok() == false)
        {
            ERR("创建租约失败: {}", rsp.error_message());
            return false;
        }
        // 获取租约id
        auto lease_id = rsp.value().lease();
        // 添加数据
        auto rsp2 = etcd_cli.put(create_key(), node_addr, lease_id).get();
        
        if(rsp2.is_ok() == false)
        {
            ERR("添加数据失败: {}", rsp2.error_message());
            return false;
        }
        // 注册成功, 生成保活对象
        auto self = shared_from_this();
        auto keepalive_hdl = [self, this,&node_addr,&method](std::exception_ptr eptr) {
            // 实现断线重连的功能
            if(register_service(node_addr, method))
            {
                INF("续约成功, 地址: {}", node_addr);
            }
            else
            {
                ERR("续约失败, 地址: {}", node_addr);
            }
        };
        keepalive_.reset(new etcd::KeepAlive(etcd_cli,keepalive_hdl, DAFAULT_LEASE_TIME, lease_id));

        return true;
    }
    
    // 注销服务
    bool ServiceProvider::deregister_service()
    {
        auto lease_id = keepalive_->Lease();
        // 实例化客户端
        auto etcd_cli = etcd::Client(rigister_addr_);

        auto rsp = etcd_cli.leaserevoke(lease_id).get();

        if(rsp.is_ok() == false)
        {
            ERR("注销租约失败: {}", rsp.error_message());
            return false;
        }
        else return true;
    }


    std::string ServiceProvider::create_key()
    {
        // "/service_name/node_id"
        return "/" + service_name_ + "/" + node_id_;
    }

    // 构造函数
    ServiceDiscovery::ServiceDiscovery(const std::string& rigister_addr,
            service_online_callback_t&& online_cb, 
            service_offline_callback_t&& offline_cb) : 
        rigister_addr_(rigister_addr),
        online_cb_(std::move(online_cb)),
        offline_cb_(std::move(offline_cb))
    {}
    

    std::string ServiceDiscovery::parse_key(const std::string& key)
    {
        // 从key中解析出服务的名称
        auto left = key.find_first_of('/');
        auto right = key.find_last_of('/');
        if(left == std::string::npos || right == std::string::npos)
        {
            ERR("解析key失败: {}", key);
            return "";
        }
        return key.substr(left+1, right-left-1);
    }
    // 监测
    void ServiceDiscovery::watch_callback(const etcd::Response& rsp)
    {
        if(rsp.is_ok() == false)
        {
            ERR("watch失败: {}", rsp.error_message());
            return;
        }
        auto const& events = rsp.events();
        for(auto const& event : events)
        {
            const auto& type = event.event_type();
            if(type == etcd::Event::EventType::PUT)
            {
                // std::cout << "put event:\n" << 
                //     "prev value:" << event.prev_kv().as_string() << "\n" <<
                //     "current value:" << event.kv().as_string() << std::endl;
                INF("节点更新\nprev value: {} -> current value: {}", 
                    event.prev_kv().as_string(), event.kv().as_string());
                const std::string& svc_name = parse_key(event.kv().key());
                const std::string& svr_addr = event.kv().as_string();
                // 新的服务上线了
                if(online_cb_)
                    online_cb_(svc_name, svr_addr);
            }
            else if(type == etcd::Event::EventType::DELETE_)
            {
                INF("节点删除\nprev value: {} -> current value: {}", 
                    event.prev_kv().as_string(), event.kv().as_string());
                const std::string& svc_name = parse_key(event.kv().key());
                const std::string& svr_addr = event.kv().as_string();
                // 新的服务上线了
                if(offline_cb_)
                    offline_cb_(svc_name, svr_addr);
            }
            else 
            {
                INF("无效的事件类型");
            }
        }
    }

    bool ServiceDiscovery::watch_service()
    {
        // 获取客户端对象
        auto etcd_cli = etcd::Client(rigister_addr_);
        // 等待连接成功
        wait_for_connection(etcd_cli);
        // 构建watcher
        auto rsp = etcd_cli.ls("/").get();
        if(rsp.is_ok() == false)
        {
            ERR("获取服务列表失败: {}", rsp.error_message());
            return false;
        }

        auto& values = rsp.values();
        for(auto& value : values)
        {
            const std::string& method = parse_key(value.key());
            const std::string& svr_addr = value.as_string();
            if(online_cb_)
                online_cb_(method, svr_addr);
        }
        auto self = shared_from_this();
        watcher_.reset(new etcd::Watcher(rigister_addr_, "/", 
            [self](const etcd::Response& rsp){ self->watch_callback(rsp); }, true));

        return true;
    }

}
