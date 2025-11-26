#include "server.hpp"

// stlcpp
#include <memory>
#include <ranges>

// stlc
#include <cerrno>
#include <cstddef>
#include <cstring>
#include "command/command.hpp"

// unix
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>


namespace async_server {


Server::Server(std::unique_ptr<TcpSocket>&& tcp_socket,
               std::unique_ptr<UdpSocket>&& udp_socket,
               int epoll_fd,
               std::unordered_map<int, FdType> fd_info)
    : m_stats()
    , m_tcp_socket(std::move(tcp_socket))
    , m_udp_socket(std::move(udp_socket))
    , m_epoll_fd(epoll_fd)
    , m_fd_info(std::move(fd_info)) {
}

Server::~Server() {
    for (auto& [fd, fd_type] : m_fd_info) {
        if (fd_type != FdType::TcpListener && fd_type != FdType::UdpSocket) {
            close(fd);
        }
    }
    
    if (m_epoll_fd >= 0) {
        close(m_epoll_fd);
    }
}

std::optional<std::string> Server::Run() {
    epoll_event events[kMaxEventCount];
    
    while (m_running.load(std::memory_order_relaxed)) {
        int nfds = epoll_wait(m_epoll_fd, events, kMaxEventCount, 1000);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            return std::strerror(errno);
        }
        
        for (auto& event : events | std::views::take(nfds)) {
            int fd = event.data.fd;
            auto it = m_fd_info.find(fd);
            if (it == m_fd_info.end()) {
                continue;
            }
            
            std::optional<std::string> process_status = std::nullopt;
            try {
                switch (it->second) {
                    case FdType::Signal:
                        Stop();
                        break;
                    case FdType::TcpListener:
                        process_status = handleTcpAccept();
                        break;
                    case FdType::UdpSocket:
                        process_status = handleUdpMessage();
                        break;
                    case FdType::TcpClient:
                        process_status = handleTcpClient(fd);
                        break;
                }
            } catch (const std::exception& e) {
                return e.what();
            }

            if (process_status) {
                return process_status;
            }
        }
    }

    return std::nullopt;
}

void Server::Stop() {
    m_running.store(false, std::memory_order_relaxed);
}

std::optional<std::string> Server::handleTcpAccept() {
    while (true) {
        auto client_result = m_tcp_socket->accept_connection();
        if (!client_result) {
            return client_result.error();
        }
        
        int client_fd = client_result.value().first;
        if (client_fd == -1) {
            break;
        }
        
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        
        m_fd_info.emplace(client_fd, FdType::TcpClient);
        
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            m_fd_info.erase(client_fd);
            close(client_fd);
            continue;
        }
        m_stats.increment_total();
        m_stats.increment_current();
    }
    
    return std::nullopt;
}

std::optional<std::string> Server::handleTcpClient(int client_fd) {
    auto close_connection = [this](int* client_fd_ptr){
        if (!client_fd_ptr) {
            return;
        }

        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, *client_fd_ptr, nullptr);
        close(*client_fd_ptr);
        m_fd_info.erase(*client_fd_ptr);
        m_stats.decrement_current();
    };
    std::unique_ptr<int, decltype(close_connection)> scope_guard(&client_fd, close_connection);

    while (true) {
        char buffer[kBufferSize];
        ssize_t count = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                [[maybe_unused]] auto _ = scope_guard.release();
                return std::nullopt;
            }
            if (errno == EINTR) {
                continue;
            }

            return std::string("TCP recv error: ") + std::strerror(errno);

        } else if (count == 0) {
            break;

        } else {
            buffer[count] = '\0';
            std::string message(buffer, count);

            std::string response = CommandProcessor(m_stats, m_running).process(message);
            ssize_t sent = send(client_fd, response.c_str(), response.length(), MSG_NOSIGNAL);
            
            if (sent == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    break;
                }
                return std::string("TCP send error: ") + std::strerror(errno);
            }
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> Server::handleUdpMessage() {
    while (true) {
        char buffer[kBufferSize];

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        ssize_t count = recvfrom(m_udp_socket->fd(), buffer, sizeof(buffer) - 1, MSG_TRUNC,
                                  (sockaddr*)&client_addr, &client_len);
        
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            return std::string("UDP recvfrom error: ") + std::strerror(errno);
        }
        
        if (count == 0) {
            continue;
        }
        
        if (count >= static_cast<ssize_t>(sizeof(buffer))) {
            static const std::string error_msg = "Error: Message too large (max " + 
                                    std::to_string(sizeof(buffer) - 1) + " bytes)";
            ssize_t sent = sendto(m_udp_socket->fd(), error_msg.c_str(), error_msg.length(), MSG_NOSIGNAL,
                                  (sockaddr*)&client_addr, client_len);
            if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                return std::string("UDP sendto error: ") + std::strerror(errno);
            }
            continue;
        }
        
        buffer[count] = '\0';
        std::string message(buffer, count);
        std::string response = CommandProcessor(m_stats, m_running).process(message);
        
        ssize_t sent = sendto(m_udp_socket->fd(), response.c_str(), response.length(), MSG_NOSIGNAL,
                              (sockaddr*)&client_addr, client_len);
        if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            return std::string("UDP sendto error: ") + std::strerror(errno);
        }
    }
    
    return std::nullopt;
}


} // namespace async_server
