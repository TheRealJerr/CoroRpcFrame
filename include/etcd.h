#pragma once
#include "log.h"
#include <command.h>
#include <util.h>
#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Watcher.hpp>
#include <thread>

// 针对etcd api封装相关的工具类

/*
    1. 将添加数据和数据保活结合在一次, 并且设置回调函数, 保证了监控的断线重连
    2. 将数据获取和数据监听结合在一起, 保证了数据的实时性
    3. etcd中存储 (key)服务(目录)/id(服务编号) : value(对应的ip和port)  
*/
namespace Tools
{

    // 等待连接成功
    void wait_for_connection(etcd::Client& etcd_cli);

    // 保活对象
    using etcd_keepalive_ptr = std::shared_ptr<etcd::KeepAlive>;
    // 检测对象
    using etcd_watcher_ptr = std::shared_ptr<etcd::Watcher>;
    
    // 对象的工厂类
    class EtcdObjectFactory
    {
    public:
        template <typename T,typename ...Args>
        inline static std::shared_ptr<T> create(Args&&... args)
        {
            return std::make_shared<T>(std::forward<Args>(args)...);    
        }
    };

    // 服务的提供者
    class ServiceProvider : public std::enable_shared_from_this<ServiceProvider>
    {
    public:
        ServiceProvider(const std::string& rigister_addr);
        
        // 注册服务
        bool register_service(const std::string& node_addr, 
            const std::string& method);
        
        // 注销服务
        bool deregister_service();

        // 析构的时候注销服务
        ~ServiceProvider() { deregister_service(); }
    private:
        

        std::string create_key();
            
    private:
        // 注册中心的地址
        std::string rigister_addr_; 
        // 节点的标识
        std::string node_id_;
        // 节点的的地址
        std::string node_addr_;

        // 注册的服务
        std::string service_name_;
        // 服务的保活
        etcd_keepalive_ptr keepalive_;
    };     

    // 服务的发现者
    class ServiceDiscovery : public std::enable_shared_from_this<ServiceDiscovery>
    {
    public:
        // 服务下线的操作
        // @param : 服务名称和节点地址
        using service_offline_callback_t = std::function<void(const std::string&, const std::string&)>;
        // 服务上线的操作
        // @param : 服务名称和节点地址
        using service_online_callback_t = std::function<void(const std::string&, const std::string&)>;
        
        // 构造函数
        ServiceDiscovery(const std::string& rigister_addr,
            service_online_callback_t&& online_cb, 
            service_offline_callback_t&& offline_cb);

        // 监测对应的服务
        // 根据服务的状态调用对应的回调函数
        bool watch_service();
        
    private:
        std::string parse_key(const std::string& key);
        // watcher调用的回调函数
        void watch_callback(const etcd::Response& rsp);
    private:
        // 注册中心的地址
        std::string rigister_addr_;
        // 服务的发现对象
        service_offline_callback_t offline_cb_;

        service_online_callback_t online_cb_;

        // 服务上下线的监控
        etcd_watcher_ptr watcher_;
    };

}