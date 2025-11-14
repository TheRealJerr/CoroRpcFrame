#include "../include/net.h"
#include <algorithm>
#include <cstring>

namespace net
{

// Buffer 实现
Buffer::Buffer(size_t size) : buffer_(size) {}

std::string Buffer::read_as_string(size_t size)
{
    size = std::min(size, readable_size());
    std::string result(buffer_.data() + read_pos_, size);
    read_pos_ += size;

    // 如果所有数据都已读取，重置位置
    if (read_pos_ == write_pos_) {
        read_pos_ = 0;
        write_pos_ = 0;
    }

    return result;
}

size_t Buffer::capacity() const { return buffer_.size(); }

void Buffer::resize(size_t size) 
{ 
    if (size > buffer_.size()) {
        buffer_.resize(size);
    }
}

void Buffer::ensure_capacity(size_t required_size) {
    if (writable_size() < required_size) {
        // 先尝试移动数据到开头
        if (read_pos_ > 0) {
            size_t data_size = readable_size();
            if (data_size > 0) {
                std::memmove(buffer_.data(), buffer_.data() + read_pos_, data_size);
            }
            read_pos_ = 0;
            write_pos_ = data_size;
        }

        // 如果仍然不够空间，则扩容
        if (writable_size() < required_size) {
            size_t new_size = std::max(buffer_.size() * 2, write_pos_ + required_size);
            buffer_.resize(new_size);
        }
    }
}

std::string Buffer::read_all_as_string()
{
    return read_as_string(readable_size());
}

void Buffer::append(const std::string& data)
{
    append(data.data(), data.size());
}

void Buffer::append(const char* data, size_t size)
{
    if (size == 0) return;
    
    ensure_capacity(size);
    std::copy(data, data + size, buffer_.data() + write_pos_);
    write_pos_ += size;
}

void Buffer::clear()
{
    read_pos_ = 0;
    write_pos_ = 0;
}

bool Buffer::empty() const { return readable_size() == 0; }

char* Buffer::write_data() { return buffer_.data() + write_pos_; }

const char* Buffer::read_data() const { return buffer_.data() + read_pos_; }

size_t Buffer::writable_size() const {
    return buffer_.size() - write_pos_;
}

size_t Buffer::readable_size() const {
    return write_pos_ - read_pos_;
}

void Buffer::advance_write(size_t size) {
    write_pos_ += size;
}

void Buffer::advance_read(size_t size) {
    read_pos_ += size;
    if (read_pos_ == write_pos_) {
        read_pos_ = 0;
        write_pos_ = 0;
    }
}

// Session 实现
Session::Session(asio::io_context& io_context,
                 ip::tcp::socket socket,
                 OnMsgCallback on_msg_callback) :
    socket_(std::move(socket)),
    cb_(std::move(on_msg_callback))
{
    read_.resize(8192); // 初始8KB缓冲区
}

void Session::start()
{
    INF("Session started with remote: {}", 
        socket_.remote_endpoint().address().to_string());
    do_read();
}

void Session::close() {
    boost::system::error_code ec;
    socket_.close(ec);
    if (ec) {
        ERR("Session close error: {}", ec.message());
    }
}

void Session::do_read() {
    auto self = shared_from_this();
    
    read_.ensure_capacity(1024); // 确保有足够空间
    
    socket_.async_read_some(
        asio::buffer(read_.write_data(), read_.writable_size()),
        [this, self](boost::system::error_code ec, std::size_t n) {
            if (ec) {
                if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
                    ERR("Session read error: {}", ec.message());
                }
                return;
            }

            INF("Session read size: {}", n);
            read_.advance_write(n);

            // 处理消息
            Buffer send_buf;
            if (cb_) {
                try {
                    cb_(read_, &send_buf);
                } catch (const std::exception& e) {
                    ERR("Message callback error: {}", e.what());
                }
            }

            // 如果有响应数据，发送回去
            if (send_buf.readable_size() > 0) {
                do_write(send_buf.read_all_as_string());
            } else {
                // 继续读取下一条消息
                do_read();
            }
        });
}

void Session::do_write(const std::string& data) {
    auto self = shared_from_this();
    
    asio::async_write(
        socket_,
        asio::buffer(data),
        [this, self](boost::system::error_code ec, std::size_t /*n*/) {
            if (ec) {
                ERR("Session write error: {}", ec.message());
                return;
            }
            
            // 写入完成后继续读取
            do_read();
        });
}

// Server 实现
Server::Server(asio::io_context& ioc, int port, OnMsgCallback cb) :
    ioc_(ioc),
    acceptor_(ioc_, ip::tcp::endpoint(ip::tcp::v4(), port)),
    cb_(std::move(cb))
{
}

void Server::start()
{
    INF("Server started on port: {}", acceptor_.local_endpoint().port());
    do_accept();
}

void Server::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
    if (ec) {
        ERR("Server stop error: {}", ec.message());
    }
}

void Server::do_accept() {
    auto self = shared_from_this();
    
    acceptor_.async_accept(
        [this, self](boost::system::error_code ec, ip::tcp::socket socket) {
            if (ec) {
                ERR("Accept error: {}", ec.message());
            } else {
                try {
                    INF("New connection from: {}", 
                        socket.remote_endpoint().address().to_string());
                        
                    auto session = std::make_shared<Session>(
                        ioc_, std::move(socket), cb_);
                    session->start();
                } catch (const std::exception& e) {
                    ERR("Session creation error: {}", e.what());
                }
            }
            
            // 继续接受新连接
            do_accept();
        });
}

// Client 实现
Client::Client(asio::io_context& ioc, 
               const std::string& host, 
               const std::string& port, 
               CliOnMsgCallback cb) :
    ioc_(ioc),
    resolver_(ioc_),
    host_(host),
    port_(port),
    socket_(ioc_),
    cb_(std::move(cb))
{
    read_.resize(8192);
}

void Client::start()
{
    INF("Client connecting to {}:{}", host_, port_);
    
    resolver_.async_resolve(host_, port_,
        [this](boost::system::error_code ec, ip::tcp::resolver::results_type results) {
            if (ec) {
                ERR("Resolve error: {}", ec.message());
                return;
            }
            do_connect(results);
        });
}

void Client::send(const std::string& data) {
    auto self = shared_from_this();
    
    asio::async_write(socket_, asio::buffer(data),
        [this, self](boost::system::error_code ec, std::size_t /*n*/) {
            if (ec) {
                ERR("Client send error: {}", ec.message());
            }
        });
}

void Client::close() {
    boost::system::error_code ec;
    socket_.close(ec);
    if (ec) {
        ERR("Client close error: {}", ec.message());
    }
}

void Client::do_connect(const ip::tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    
    asio::async_connect(socket_, endpoints,
        [this, self](boost::system::error_code ec, ip::tcp::endpoint) {
            if (ec) {
                ERR("Connect error: {}", ec.message());
                return;
            }
            
            INF("Connected to server: {}:{}", host_, port_);
            do_read();
        });
}

void Client::do_read() {
    auto self = shared_from_this();
    
    read_.ensure_capacity(1024);
    
    socket_.async_read_some(
        asio::buffer(read_.write_data(), read_.writable_size()),
        [this, self](boost::system::error_code ec, std::size_t n) {
            if (ec) {
                if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
                    ERR("Client read error: {}", ec.message());
                }
                return;
            }

            INF("Client read size: {}", n);
            read_.advance_write(n);

            // 处理接收到的消息
            if (cb_) {
                try {
                    cb_(read_);
                } catch (const std::exception& e) {
                    ERR("Client message callback error: {}", e.what());
                }
            }
            
            // 继续读取
            do_read();
        });
}

} // namespace net