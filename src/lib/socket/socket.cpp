#include "socket.hpp"

// stlcpp   
#include <expected>
#include <memory>
#include <string>

// stlc
#include <cerrno>
#include <cstdio>
#include <cstring>

// unix
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>


namespace async_server {

Socket::~Socket() {
    if (m_fd >= 0) {
        close(m_fd);
    }
}

Socket::Socket(Socket&& other) noexcept : m_fd(other.m_fd) {
    other.m_fd = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (m_fd >= 0) {
        close(m_fd);
    }
    m_fd = other.m_fd;
    other.m_fd = -1;

    return *this;
}

bool Socket::set_nonblocking() {
    int flags = fcntl(m_fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    
    if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return false;
    }
    
    return true;
}

TcpSocket::TcpCreateResult TcpSocket::create(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return std::unexpected("TCP socket init error");
    }
    
    auto close_sock = [](int* fd) {
        if (fd && *fd != -1) {
            close(*fd);
        }
    };
    std::unique_ptr<int, decltype(close_sock)> sock_guard(&sock, close_sock);
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        return std::unexpected("Error bind TCP socket on port " + std::to_string(port));
    }
    
    if (listen(sock, SOMAXCONN) == -1) {
        return std::unexpected("Listen TCP socket error");
    }
    
    TcpSocket tcp_sock(sock);
    if (!tcp_sock.set_nonblocking()) {
        return std::unexpected("Set nonblocking TCP socket error");
    }
    
    [[maybe_unused]] auto _ = sock_guard.release();
    
    return tcp_sock;
}

std::expected<std::pair<int, sockaddr_in>, std::string> TcpSocket::accept_connection() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept4(m_fd, 
                           reinterpret_cast<sockaddr*>(&client_addr), 
                           &client_len, 
                           SOCK_NONBLOCK);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::pair{-1, client_addr};
        }
        
        return std::unexpected("accept() failed: " + std::string(std::strerror(errno)));
    }

    auto close_client = [](int* fd) {
        if (fd && *fd != -1) {
            close(*fd);
        }
    };
    std::unique_ptr<int, decltype(close_client)> client_guard(&client_fd, close_client);
    
    [[maybe_unused]] auto _ = client_guard.release();
    
    return std::pair{client_fd, client_addr};
}

UdpSocket::UdpCreateResult UdpSocket::create(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return std::unexpected("UDP socket init error");
    }
    
    auto close_sock = [](int* fd) {
        if (fd && *fd != -1) {
            close(*fd);
        }
    };
    std::unique_ptr<int, decltype(close_sock)> sock_guard(&sock, close_sock);
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        return std::unexpected("Error bind UDP socket on port " + std::to_string(port));
    }
    
    UdpSocket udp_sock(sock);
    if (!udp_sock.set_nonblocking()) {
        return std::unexpected("Set nonblocking TCP socket error");
    }
    
    [[maybe_unused]] auto _ = sock_guard.release();
    
    return udp_sock;
}

}
