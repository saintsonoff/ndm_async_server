#pragma once

// stlcpp
#include <concepts>
#include <expected>
#include <optional>
#include <string>
#include <utility>

// unix
#include <netinet/in.h>

namespace async_server {

template <typename T>
concept FileDescriptorLike = requires(T t) {
    { t.fd() } -> std::same_as<int>;
};

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

    static constexpr int kInvalidFd = -1;

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

    std::expected<int, std::string> accept_connection();

    static constexpr int kDefaultBacklog = 128;

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

}  // namespace async_server
