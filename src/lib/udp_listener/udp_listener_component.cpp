#include "udp_listener_component.hpp"

// stlc
#include <cstring>

// stlcpp
#include <iostream>

// unix
#include <unistd.h>

namespace async_server {

UdpListenerComponent::UdpListenerComponent(EventLoop& event_loop,
                                           std::unique_ptr<UdpSocket> listener,
                                           Statistics& stats,
                                           std::atomic<bool>& shutdown_flag)
    : m_event_loop(event_loop),
      m_listener(std::move(listener)),
      m_stats(stats),
      m_command_processor(stats, shutdown_flag),
      m_health(ComponentHealth::kUnhealthy) {}

UdpListenerComponent::~UdpListenerComponent() {
    [[maybe_unused]] auto _ = Stop();
}

std::expected<void, std::string> UdpListenerComponent::Start() {
    if (!m_listener) {
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected("No listener socket");
    }

    int listener_fd = m_listener->fd();

    auto add_status = m_event_loop.AddFd(
        listener_fd, EPOLLIN | EPOLLET,
        [this](int fd, uint32_t events) { return handleUdpEvent(fd, events); });

    if (!add_status) {
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected(add_status.error());
    }

    m_health = ComponentHealth::kOk;

    return {};
}

std::expected<void, std::string> UdpListenerComponent::Stop() {
    if (m_listener) {
        if (auto remove_status = m_event_loop.RemoveFd(m_listener->fd());
            !remove_status) {
            return std::unexpected(remove_status.error());
        }
    }

    m_health = ComponentHealth::kUnhealthy;

    return {};
}

ComponentHealth UdpListenerComponent::GetComponentHealth() const noexcept {
    return m_health;
}

std::expected<void, std::string> UdpListenerComponent::handleUdpEvent(
    [[maybe_unused]] int fd, uint32_t events) {
    if (events & (EPOLLERR | EPOLLHUP)) {
        m_health = ComponentHealth::kDegraded;
        return std::unexpected<std::string>("Error on listener socket");
    }

    if (events & EPOLLIN) {
        char buffer[kBufferSize + 1];
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        while (true) {
            ssize_t read_size = recvfrom(
                m_listener->fd(), buffer, sizeof(buffer), MSG_TRUNC,
                reinterpret_cast<sockaddr*>(&client_addr), &client_len);

            if (read_size < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                m_health = ComponentHealth::kDegraded;
                return std::unexpected<std::string>(std::strerror(errno));
            }

            std::string response;
            if (static_cast<size_t>(read_size) > kBufferSize) {
                response = "Error: Message too large";
            } else {
                std::string message(buffer, static_cast<size_t>(read_size));
                response = m_command_processor.process(message);
            }

            ssize_t sent =
                sendto(m_listener->fd(), response.c_str(), response.size(), 0,
                       reinterpret_cast<sockaddr*>(&client_addr), client_len);

            if (sent < 0) {
                return std::unexpected<std::string>(std::strerror(errno));
            }
        }
    }

    return {};
}

}  // namespace async_server
