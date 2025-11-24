#include "server.hpp"

// stlcpp
#include <memory>

// stlc
#include <cerrno>
#include <cstddef>
#include <cstring>

// unix
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>


namespace async_server {


Server::Server(Config config) 
    : m_config(std::move(config))
    , m_command_processor(m_stats, m_running) {
}

Server::~Server() {
    if (m_signal_fd >= 0) {
        if (m_epoll_fd >= 0) {
            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, m_signal_fd, nullptr);
        }
    }
    
    if (m_epoll_fd >= 0) {
        close(m_epoll_fd);
    }
    
    for (auto& [fd, info] : m_fd_info) {
        if (info->type == FdType::TcpClient) {
            close(fd);
        }
    }
}

bool Server::Initialize() {
    auto tcp_result = TcpSocket::create(m_config.GetTcpPort());
    if (!tcp_result) {
        return false;
    }
    m_tcp_socket = std::make_unique<TcpSocket>(std::move(*tcp_result));
    
    auto udp_result = UdpSocket::create(m_config.GetUdpPort());
    if (!udp_result) {
        return false;
    }
    m_udp_socket = std::make_unique<UdpSocket>(std::move(*udp_result));
    
    m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epoll_fd == -1) {
        return false;
    }
    
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    
    auto tcp_info = std::make_unique<FdInfo>(FdInfo{m_tcp_socket->fd(), FdType::TcpListener});
    ev.data.ptr = tcp_info.get();
    
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_tcp_socket->fd(), &ev) == -1) {
        return false;
    }
    m_fd_info[m_tcp_socket->fd()] = std::move(tcp_info);
    
    auto udp_info = std::make_unique<FdInfo>(FdInfo{m_udp_socket->fd(), FdType::UdpSocket});
    ev.data.ptr = udp_info.get();
    
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_udp_socket->fd(), &ev) == -1) {
        return false;
    }
    m_fd_info[m_udp_socket->fd()] = std::move(udp_info);
    
    return true;
}

void Server::AddSignalFd(int signal_fd) {
    m_signal_fd = signal_fd;
    
    if (m_epoll_fd < 0 || signal_fd < 0) {
        return;
    }
    
    epoll_event ev{};
    ev.events = EPOLLIN;
    
    auto sig_info = std::make_unique<FdInfo>(FdInfo{signal_fd, FdType::Signal});
    ev.data.ptr = sig_info.get();
    
    epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, signal_fd, &ev);
    m_fd_info[signal_fd] = std::move(sig_info);
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
            FdInfo* info = static_cast<FdInfo*>(events[i].data.ptr);
            
            try {
                switch (info->type) {
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
                        handleTcpClient(info->fd);
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
        
        auto client_info = std::make_unique<FdInfo>(FdInfo{client_fd, FdType::TcpClient});
        ev.data.ptr = client_info.get();
        
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            close(client_fd);
            continue;
        }
        
        m_fd_info[client_fd] = std::move(client_info);
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
        
        ssize_t count = recvfrom(m_udp_socket->fd(), buffer, sizeof(buffer) - 1, 0,
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
        
        buffer[count] = '\0';
        std::string message(buffer, count);
        std::string response = m_command_processor.process(message);
        
        sendto(m_udp_socket->fd(), response.c_str(), response.length(), MSG_NOSIGNAL,
               (sockaddr*)&client_addr, client_len);
    }
}


} // namespace async_server
