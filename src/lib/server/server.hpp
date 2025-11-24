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


enum class FdType : uint8_t {
    Signal,
    TcpListener,
    UdpSocket,
    TcpClient
};

struct FdInfo {
    int fd;
    FdType type;
};


class Server {
public:
    explicit Server(Config config);
    ~Server();
    
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
public:
    bool Initialize();
    void Run();
    void Stop();
    void AddSignalFd(int signal_fd);
    
private:
    void handleTcpAccept();
    void handleTcpClient(int client_fd);
    void handleUdpMessage();
    
    Config m_config;
    Statistics m_stats;
    std::atomic<bool> m_running{true};
    CommandProcessor m_command_processor;
    
    std::unique_ptr<TcpSocket> m_tcp_socket;
    std::unique_ptr<UdpSocket> m_udp_socket;
    
    int m_epoll_fd = -1;
    int m_signal_fd = -1;
    std::unordered_map<int, std::unique_ptr<FdInfo>> m_fd_info;
    
    static constexpr int kMaxEventCount = 64;
    static constexpr int kBufferSize = 4096;
};


} // namespace async_server
