#pragma once

#include "command.h"
#include "log.h"
#include <functional>
#include <boost/asio.hpp>

namespace ip = boost::asio::ip;
namespace asio = boost::asio;

namespace net
{

class Buffer
{
public:
    Buffer() = default;
    Buffer(size_t size);
    
    std::string read_as_string(size_t size);
    size_t capacity() const;
    void resize(size_t size);
    void ensure_capacity(size_t required_size);
    std::string read_all_as_string();
    void append(const std::string& data);
    void append(const char* data, size_t size);
    void clear();
    bool empty() const;
    char* write_data();
    const char* read_data() const;
    size_t writable_size() const;
    size_t readable_size() const;
    void advance_write(size_t size);
    void advance_read(size_t size);

private:
    std::vector<char> buffer_;
    size_t read_pos_{ 0 };
    size_t write_pos_{ 0 };
};

using OnMsgCallback = std::function<void(Buffer& recv, Buffer* send)>;
using CliOnMsgCallback = std::function<void(Buffer& recv)>;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(asio::io_context& io_context,
            ip::tcp::socket socket,
            OnMsgCallback on_msg_callback);
    void start();
    void close();

private:
    void do_read();
    void do_write(const std::string& data);

    Buffer read_;
    ip::tcp::socket socket_;
    OnMsgCallback cb_;
};

class Server : public std::enable_shared_from_this<Server>
{
public:
    Server(asio::io_context& ioc, int port, OnMsgCallback cb);
    void start();
    void stop();

private:
    void do_accept();

    asio::io_context& ioc_;
    asio::ip::tcp::acceptor acceptor_;
    OnMsgCallback cb_;
};

class Client : public std::enable_shared_from_this<Client>
{
public:
    Client(asio::io_context& ioc, 
           const std::string& host, 
           const std::string& port, 
           CliOnMsgCallback cb);
    void start();
    void send(const std::string& data);
    void close();

private:
    void do_connect(const ip::tcp::resolver::results_type& endpoints);
    void do_read();

    asio::io_context& ioc_;
    asio::ip::tcp::resolver resolver_;
    std::string host_;
    std::string port_;
    asio::ip::tcp::socket socket_;
    CliOnMsgCallback cb_;
    Buffer read_;
};

} // namespace net
