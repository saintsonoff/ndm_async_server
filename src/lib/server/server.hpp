#pragma once

// stlcpp
#include <memory>
#include <atomic>
#include <unordered_map>

// unix
#include <sys/epoll.h>

// self
#include <socket/socket.hpp>
#include <command/command.hpp>
#include <config/config.hpp>


namespace async_server {


class Server {
public:
    explicit Server(Config config);
    ~Server();
    
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    Result<void*> initialize();
    void run();
    void stop();
    void add_signal_fd(int signal_fd);
    
private:
    void handle_tcp_accept();
    void handle_tcp_client(int client_fd);
    void handle_udp_message();
    
    Config config_;
    Statistics stats_;
    std::atomic<bool> running_{true};
    CommandProcessor processor_;
    
    std::unique_ptr<TcpSocket> tcp_socket_;
    std::unique_ptr<UdpSocket> udp_socket_;
    
    int epoll_fd_{-1};
    int signal_fd_{-1};
    std::unordered_map<int, bool> clients_;
    
    static constexpr int MAX_EVENTS = 64;
    static constexpr int BUFFER_SIZE = 4096;
};


} // namespace async_server
