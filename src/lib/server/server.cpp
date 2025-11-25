#include "server.hpp"

// stlcpp
#include <memory>

// stlc
#include <cerrno>
#include <cstddef>
#include <cstring>

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
               std::unordered_map<int, std::pair<int, FdType>> fd_info)
    : m_stats()
    , m_command_processor(m_stats, m_running)
    , m_tcp_socket(std::move(tcp_socket))
    , m_udp_socket(std::move(udp_socket))
    , m_epoll_fd(epoll_fd)
    , m_fd_info(std::move(fd_info)) {
}

Server::~Server() {
    for (auto& [fd, info] : m_fd_info) {
        if (info.second != FdType::TcpListener && info.second != FdType::UdpSocket) {
            close(fd);
        }
    }
    
    if (m_epoll_fd >= 0) {
        close(m_epoll_fd);
    }
}

void Server::Run() {
    epoll_event events[kMaxEventCount];
    
    while (m_running.load(std::memory_order_relaxed)) {
        int nfds = epoll_wait(m_epoll_fd, events, kMaxEventCount, 1000);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            auto* info = static_cast<std::pair<int, FdType>*>(events[i].data.ptr);
            
            try {
                switch (info->second) {
                    case FdType::Signal:
                        Stop();
                        break;
                    case FdType::TcpListener:
                        handleTcpAccept();
                        break;
                    case FdType::UdpSocket:
                        handleUdpMessage();
                        break;
                    case FdType::TcpClient:
                        handleTcpClient(info->first);
                        break;
                }
            } catch (const std::exception& e) {
                // logging exception
            } catch (...) {
                // unknowing exception
            }
        }
    }
}

void Server::Stop() {
    m_running.store(false, std::memory_order_relaxed);
}

void Server::handleTcpAccept() {
    while (true) {
        auto client_result = m_tcp_socket->accept_connection();
        if (!client_result) {
            break;
        }
        
        int client_fd = *client_result;
        
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        
        auto [it, inserted] = m_fd_info.emplace(client_fd, std::make_pair(client_fd, FdType::TcpClient));
        ev.data.ptr = &it->second;
        
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            m_fd_info.erase(it);
            close(client_fd);
            continue;
        }
        m_stats.increment_total();
        m_stats.increment_current();
    }
}

void Server::handleTcpClient(int client_fd) {
    auto close_connection_f = [this](int* client_fd_ptr){
        if (!client_fd_ptr) {
            return;
        }

        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, *client_fd_ptr, nullptr);
        close(*client_fd_ptr);
        m_fd_info.erase(*client_fd_ptr);
        m_stats.decrement_current();
    };
    std::unique_ptr<int, decltype(close_connection_f)> scope_guard(&client_fd, close_connection_f);

    while (true) {
        char buffer[kBufferSize];
        ssize_t count = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                [[maybe_unused]] auto _ = scope_guard.release();
                return;
            }
            if (errno == EINTR) {
                continue;
            }

            // process bad error
            break;

        } else if (count == 0) {
            break;

        } else {
            buffer[count] = '\0';
            std::string message(buffer, count);

            std::string response = m_command_processor.process(message);   
            send(client_fd, response.c_str(), response.length(), MSG_NOSIGNAL);
        }
    }
}

void Server::handleUdpMessage() {
    char buffer[kBufferSize];
    
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        ssize_t count = recvfrom(m_udp_socket->fd(), buffer, sizeof(buffer) - 1, MSG_TRUNC,
                                  (sockaddr*)&client_addr, &client_len);
        
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            // process bad error
            break;
        }
        
        if (count == 0) {
            continue;
        }
        
        if (count >= static_cast<ssize_t>(sizeof(buffer))) {
            std::string error_msg = "Error: Message too large (max " + 
                                    std::to_string(sizeof(buffer) - 1) + " bytes)";
            sendto(m_udp_socket->fd(), error_msg.c_str(), error_msg.length(), MSG_NOSIGNAL,
                   (sockaddr*)&client_addr, client_len);
            continue;
        }
        
        buffer[count] = '\0';
        std::string message(buffer, count);
        std::string response = m_command_processor.process(message);
        
        sendto(m_udp_socket->fd(), response.c_str(), response.length(), MSG_NOSIGNAL,
               (sockaddr*)&client_addr, client_len);
    }
}


} // namespace async_server
