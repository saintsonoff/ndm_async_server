#include "socket.hpp"

// stlc
#include <cerrno>

// unix
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>


namespace async_server {

Socket::~Socket() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (fd_ >= 0) {
        close(fd_);
    }
    fd_ = other.fd_;
    other.fd_ = -1;

    return *this;
}

bool Socket::set_nonblocking() {
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        return false;
    }
    
    return true;
}

std::optional<TcpSocket> TcpSocket::create(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return std::nullopt;
    }
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sock);
        return std::nullopt;
    }
    
    if (listen(sock, SOMAXCONN) == -1) {
        close(sock);
        return std::nullopt;
    }
    
    TcpSocket tcp_sock(sock);
    if (tcp_sock.set_nonblocking()) {
        return std::nullopt;
    }
    
    return tcp_sock;
}

std::optional<int> TcpSocket::accept_connection() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;
        }
        return std::nullopt;
    }
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    }
    
    return client_fd;
}

std::optional<UdpSocket> UdpSocket::create(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return std::nullopt;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sock);
        return std::nullopt;
    }
    
    UdpSocket udp_sock(sock);
    if (udp_sock.set_nonblocking()) {
        return std::nullopt;
    }
    
    return udp_sock;
}

}
