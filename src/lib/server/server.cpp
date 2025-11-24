#include "server.hpp"

// stlc
#include <cerrno>
#include <cstring>

// unix
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>


namespace async_server {


Server::Server(Config config) 
    : config_(std::move(config))
    , processor_(stats_, running_) {
}

Server::~Server() {
    if (signal_fd_ >= 0) {
        if (epoll_fd_ >= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, signal_fd_, nullptr);
        }
    }
    
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
    
    for (auto& [fd, _] : clients_) {
        close(fd);
    }
}

Result<void*> Server::initialize() {
    auto tcp_result = TcpSocket::create(config_.get_tcp_port());
    if (!tcp_result) {
        return std::nullopt;
    }
    tcp_socket_ = std::make_unique<TcpSocket>(std::move(*tcp_result));
    
    auto udp_result = UdpSocket::create(config_.get_udp_port());
    if (!udp_result) {
        return std::nullopt;
    }
    udp_socket_ = std::make_unique<UdpSocket>(std::move(*udp_result));
    
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        return std::nullopt;
    }
    
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = tcp_socket_->fd();
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tcp_socket_->fd(), &ev) == -1) {
        return std::nullopt;
    }
    
    ev.data.fd = udp_socket_->fd();
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, udp_socket_->fd(), &ev) == -1) {
        return std::nullopt;
    }
    
    return nullptr;
}

void Server::add_signal_fd(int signal_fd) {
    signal_fd_ = signal_fd;
    
    if (epoll_fd_ < 0 || signal_fd < 0) {
        return;
    }
    
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = signal_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, signal_fd, &ev);
}

void Server::run() {
    epoll_event events[MAX_EVENTS];
    
    while (running_.load(std::memory_order_relaxed)) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
        
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (fd == signal_fd_) {
                stop();
            } else if (fd == tcp_socket_->fd()) {
                handle_tcp_accept();
            } else if (fd == udp_socket_->fd()) {
                handle_udp_message();
            } else {
                handle_tcp_client(fd);
            }
        }
    }
}

void Server::stop() {
    running_.store(false, std::memory_order_relaxed);
}

void Server::handle_tcp_accept() {
    while (true) {
        auto client_result = tcp_socket_->accept_connection();
        if (!client_result) {
            break;
        }
        
        int client_fd = *client_result;
        
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            close(client_fd);
            continue;
        }
        
        clients_[client_fd] = true;
        stats_.increment_total();
        stats_.increment_current();
    }
}

void Server::handle_tcp_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    
    while (true) {
        ssize_t count = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
            close(client_fd);
            clients_.erase(client_fd);
            stats_.decrement_current();
            break;
        } else if (count == 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
            close(client_fd);
            clients_.erase(client_fd);
            stats_.decrement_current();
            break;
        } else {
            buffer[count] = '\0';
            std::string message(buffer, count);
            std::string response = processor_.process(message);
            
            send(client_fd, response.c_str(), response.length(), 0);
        }
    }
}

void Server::handle_udp_message() {
    char buffer[BUFFER_SIZE];
    
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        ssize_t count = recvfrom(udp_socket_->fd(), buffer, sizeof(buffer) - 1, 0,
                                  (sockaddr*)&client_addr, &client_len);
        
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            break;
        }
        
        if (count == 0) {
            continue;
        }
        
        buffer[count] = '\0';
        std::string message(buffer, count);
        std::string response = processor_.process(message);
        
        sendto(udp_socket_->fd(), response.c_str(), response.length(), 0,
               (sockaddr*)&client_addr, client_len);
    }
}


} // namespace async_server
