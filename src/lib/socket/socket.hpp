#pragma once

// stlcpp
#include <optional>
#include <utility>
#include <expected>
#include <string>

// unix
#include <netinet/in.h>

namespace async_server {

enum class SocketError {
    CreateFailed,
    BindFailed,
    ListenFailed,
    SetNonBlockingFailed,
    AcceptFailed,
    SendFailed,
    ReceiveFailed
};


class Socket {
public:
    virtual ~Socket();
    
    int fd() const { return m_fd; }
    bool is_valid() const { return m_fd >= 0; }
    
    bool set_nonblocking();
    
protected:
    explicit Socket(int fd) : m_fd(fd) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    int m_fd = -1;
};

class TcpSocket : public Socket {
public:
    using TcpCreateResult = std::expected<TcpSocket, std::string>;

public:
    static TcpCreateResult create(int port);
    
    std::expected<std::pair<int, sockaddr_in>, std::string> accept_connection();

private:
    using Socket::Socket;
};

class UdpSocket : public Socket {
public:
    using UdpCreateResult = std::expected<UdpSocket, std::string>;

public:
    static UdpCreateResult create(int port);
    
private:
    using Socket::Socket;
};

}
