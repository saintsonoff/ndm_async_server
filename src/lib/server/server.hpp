#pragma once

// stlcpp
#include <memory>
#include <atomic>
#include <unordered_map>

// self
#include <socket/socket.hpp>
#include <command/command.hpp>
#include <config/config.hpp>


namespace async_server {


enum class FdType : uint8_t {
    Signal,
    TcpListener,
    UdpSocket,
    TcpClient
};

class ServerBuilder;

class Server final {
public:
    ~Server();
    
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    void Run();
    void Stop();
    
private:
    friend class ServerBuilder;
    
    Server(std::unique_ptr<TcpSocket>&& tcp_socket,
           std::unique_ptr<UdpSocket>&& udp_socket,
           int epoll_fd,
           std::unordered_map<int, FdType> fd_info);
    
private:
    void handleTcpAccept();
    void handleTcpClient(int client_fd);
    void handleUdpMessage();
    
    Statistics m_stats;
    std::atomic<bool> m_running{true};
    
    std::unique_ptr<TcpSocket> m_tcp_socket;
    std::unique_ptr<UdpSocket> m_udp_socket;
    
    int m_epoll_fd = -1;
    std::unordered_map<int, FdType> m_fd_info;
    
    static constexpr int kMaxEventCount = 64;
    static constexpr int kBufferSize = 8192;
};


} // namespace async_server