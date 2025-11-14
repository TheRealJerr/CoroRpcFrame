#pragma once
#include "log.h"
// 提供rpc注册中心的文件
#include <iostream>
#include <iostream>
#include <brpc/channel.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>

#include <brpc/server.h>
#include <brpc/log.h>
#include <butil/logging.h>


namespace RpcTools
{
    using namespace Tools;
    // 针对NewCallback的封装, 使得支持lambda表达式和仿函数
    struct Wrapper
    {
        Wrapper(std::function<void()> callback) : callback_(std::move(callback)) {}
        std::function<void()> callback_;

        static void static_callback(Wrapper* wrapper)
        {
            std::unique_ptr<Wrapper> wrapper_guard(wrapper);
            wrapper->callback_();
        }
    };
    // Closure的创建使用NewCallback函数, 但是这个函数的参数只支持函数指针, 因此我们针对NewCallback进行再封装, 封装了工厂类, 使得支持lambda表达式和仿函数

    // @create_closure
    class CreateClosureFactory
    {
    public:
        using callback_t = std::function<void(void)>;

        template <class Func, class... Args>
        static google::protobuf::Closure* create_closure(Func&& func, Args&&... args) {
            // 使用 lambda 捕获参数，而不是 std::bind
            auto callback = [func = std::forward<Func>(func), 
                            args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply([&](auto&&... captured_args) {
                    func(captured_args...);
                }, args_tuple);
            };
            
            Wrapper* wrapper = new Wrapper(std::move(callback));
            return google::protobuf::NewCallback(Wrapper::static_callback, wrapper);
        }
    };

    // 针对brpc::Channel进行池化的封装
    // 1. channel集合类, 一个服务可能存在多个channel可能提供服务
    // 每个服务都拥有一个自己的Channels对象, 管理自己的channel集合
    class Channels
    {
    public:
        
        using ChannelPtr_t = std::shared_ptr<brpc::Channel>;
        using ChannelMap_t = std::unordered_map<std::string, ChannelPtr_t>;
        
        // 构造函数
        Channels(const std::string& service_name) : service_name_(service_name) {}

        Channels() = default;

        // 拷贝构造
        Channels(const Channels& other) = delete;

        // 添加移动构造函数
        Channels(Channels&& other) noexcept 
            : service_name_(std::move(other.service_name_)),
            channel_map_(std::move(other.channel_map_)),
            channel_pool_(std::move(other.channel_pool_)),
            RR_index_(other.RR_index_)
        {}
        // 删除所有的节点
        void remove_all_nodes();


        ~Channels() { remove_all_nodes(); }

        // 新增节点
        void add_node(const std::string& addr);

        // 获取节点
        ChannelPtr_t get_node();

        // 移除节点
        void remove_node(const std::string& addr);
    

    private:
        std::mutex mtx_;
        ChannelMap_t channel_map_;
        int RR_index_ = 0; // 轮询算法的索引

        std::vector<ChannelPtr_t> channel_pool_;
        std::string service_name_;
    
    };


    class ChannelsPool
    {

        std::mutex mtx_; // 线程锁

        std::unordered_map<std::string, Channels> channels_map_; // 建立服务名和Channels的映射


    public:

        // 设置关注的服务列表
        void set_watch_services(const std::initializer_list<std::string>& services);
        
        // 新增节点
        void add_node(const std::string& service_name, const std::string& addr);

        // 获取节点
        Channels::ChannelPtr_t get_node(const std::string& service_name);

        // 移除节点
        void remove_node(const std::string& service_name, const std::string& addr);
    };



    
    

    // // 注册中心的定义将我们的注册中心管理的池化的Channels对象进行管理, 不暴露给上层
    // class RpcRigistry
    // {
    //     using ChannelsPool_t = std::unique_ptr<ChannelsPool>;
    //     template <typename Rsp_Msg>
    //     using on_msg_callback_t = std::function<void(brpc::Controller*, const Rsp_Msg*)>;


    //     ChannelsPool_t channels_pool_; // 管理Channels的对象
    // public:
    //     // 同时支持同步调用和异步调用
    //     // 1. 如果on_msg_callback为空, 则同步调用, 等待返回结果
    //     // 2. 如果on_msg_callback不为空, 则异步调用, 立即返回, 结果通过on_msg_callback返回
    //     // 默认这里的参数的生命周期都由用户管理
    //     // 请求调用对应rpc服务
    //     template <typename Stub>
    //     void call_method(brpc::Controller* cntl, 
    //         const std::string& service_name, 
    //         const google::protobuf::Message* req_msg,
    //         google::protobuf::Message* rsp_msg,
    //         on_msg_callback_t<google::protobuf::Message> on_msg_callback = 
    //     on_msg_callback_t<google::protobuf::Message>())
    //     {
    //         // 1. 获取channel信道

    //         auto channel = channels_pool_->get_node(service_name);
    //         if(channel == nullptr)
    //         {
    //             cntl->SetFailed(brpc::ENOSERVICE, "service not found");
    //             return;
    //         }

    //         // 2. 通过channel发送请求

    //         google::protobuf::Closure* done = nullptr;
    //         if(on_msg_callback) done = CreateClosureFactory::create_closure(std::move(on_msg_callback), cntl, rsp_msg);
    //         brpc::ClosureGuard done_guard(done);
    //         // 直接通过Stub调用发送数据
    //         Stub stub(channel.get());
    //         stub.CallMethod(cntl, req_msg, rsp_msg, done);
    //     }


    //     // 注册服务
    //     void register_service(const std::string& service_name, const std::string& addr);

    //     // 注销服务
    //     void unregister_service(const std::string& service_name, const std::string& addr);


    //     // 构造函数
    //     RpcRigistry() = default;

    //     // 拷贝构造
    //     RpcRigistry(const RpcRigistry&) = delete;


    // };

    // 创建一个RpcServer的工程类
    class RpcServerFactory
    {
    public:
        using RpcServer_t = std::shared_ptr<brpc::Server>;

        static RpcServer_t create_server(google::protobuf::Service* service,int port);
    };
}
