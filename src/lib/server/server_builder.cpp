#include "server_builder.hpp"

// stlcpp
#include <memory>
#include "server.hpp"

// unix
#include <sys/epoll.h>
#include <unistd.h>


namespace async_server {


ServerBuilder::ServerBuilder(Config config)
    : m_config(std::move(config)) {
}

ServerBuilder& ServerBuilder::WithSignalFd(int signal_fd) {
    m_signal_fd = signal_fd;
    return *this;
}

std::expected<std::unique_ptr<Server>, std::string> ServerBuilder::Build() {
    auto tcp_result = TcpSocket::create(m_config.GetTcpPort());
    if (!tcp_result) {
        return std::unexpected(tcp_result.error());
    }
    auto tcp_socket = std::make_unique<TcpSocket>(std::move(*tcp_result));
    
    auto udp_result = UdpSocket::create(m_config.GetUdpPort());
    if (!udp_result) {
        return std::unexpected(udp_result.error());
    }
    auto udp_socket = std::make_unique<UdpSocket>(std::move(*udp_result));
    
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        return std::unexpected("Failed to create epoll fd");
    }

    auto close_epoll = [](int* fd_ptr) {
        if (fd_ptr && *fd_ptr != -1) {
            close(*fd_ptr);
        }
    };
    std::unique_ptr<int, decltype(close_epoll)> epoll_guard(&epoll_fd, close_epoll);

    std::unordered_map<int, FdType> fd_info;
    
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;

    fd_info.emplace(tcp_socket->fd(), FdType::TcpListener);
    ev.data.fd = tcp_socket->fd();
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_socket->fd(), &ev) == -1) {
        return std::unexpected("Failed to add TCP socket to epoll");
    }

    fd_info.emplace(udp_socket->fd(), FdType::UdpSocket);
    ev.data.fd = udp_socket->fd();
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_socket->fd(), &ev) == -1) {
        return std::unexpected("Failed to add UDP socket to epoll");
    }

    if (m_signal_fd >= 0) {
        epoll_event sig_ev{};
        sig_ev.events = EPOLLIN;
        
        fd_info.emplace(m_signal_fd, FdType::Signal);
        sig_ev.data.fd = m_signal_fd;
        
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, m_signal_fd, &sig_ev) == -1) {
            return std::unexpected("Failed to add signal fd to epoll");
        }
    }

    [[maybe_unused]] auto _ = epoll_guard.release();

    auto server_ptr = std::unique_ptr<Server>(new Server(
        std::move(tcp_socket),
        std::move(udp_socket),
        epoll_fd,
        std::move(fd_info)
    ));

    return server_ptr;
}


} // namespace async_server
