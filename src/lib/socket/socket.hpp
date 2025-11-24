#pragma once

// stlcpp
#include <optional>
#include <utility>

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

template<typename T>
using Result = std::optional<T>;

class Socket {
public:
    virtual ~Socket();
    
    int fd() const { return fd_; }
    bool is_valid() const { return fd_ >= 0; }
    
    Result<SocketError> set_nonblocking();
    
protected:
    explicit Socket(int fd) : fd_(fd) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    int fd_ = -1;
};

class TcpSocket : public Socket {
public:
    static Result<TcpSocket> create(int port);
    
    Result<int> accept_connection();
    
private:
    using Socket::Socket;
};

class UdpSocket : public Socket {
public:
    static Result<UdpSocket> create(int port);
    
private:
    using Socket::Socket;
};

}
