#include "tcp_listener_component.hpp"

// stlc
#include <cstring>

// stlcpp
#include <iostream>
#include <memory>

// unix
#include <unistd.h>

namespace async_server {

TcpListenerComponent::TcpListenerComponent(EventLoop& event_loop,
                                           std::unique_ptr<TcpSocket> listener,
                                           Statistics& stats,
                                           std::atomic<bool>& shutdown_flag)
    : m_event_loop(event_loop),
      m_listener(std::move(listener)),
      m_stats(stats),
      m_command_processor(stats, shutdown_flag),
      m_health(ComponentHealth::kUnhealthy) {}

TcpListenerComponent::~TcpListenerComponent() {
    [[maybe_unused]] auto _ = Stop();
}

std::expected<void, std::string> TcpListenerComponent::Start() {
    if (!m_listener) {
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected("No listener socket");
    }

    int listener_fd = m_listener->fd();

    auto registered =
        m_event_loop.AddFd(listener_fd, EPOLLIN | EPOLLET,
                           [this]([[maybe_unused]] int fd, uint32_t events) {
                               return handleListenerEvent(events);
                           });

    if (!registered) {
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected(registered.error());
    }

    m_health = ComponentHealth::kOk;

    return {};
}

std::expected<void, std::string> TcpListenerComponent::Stop() {
    if (m_listener) {
        auto remove_status = m_event_loop.RemoveFd(m_listener->fd());
        if (!remove_status) {
            return std::unexpected(remove_status.error());
        }
    }

    for (const auto& [fd, _] : m_clients) {
        const char* shutdown_msg = "Server is shutting down\n";
        send(fd, shutdown_msg, strlen(shutdown_msg), MSG_NOSIGNAL);

        auto remove_status = m_event_loop.RemoveFd(fd);
        close(fd);
        if (!remove_status) {
            return std::unexpected(remove_status.error());
        }
    }
    m_clients.clear();

    m_health = ComponentHealth::kUnhealthy;

    return {};
}

ComponentHealth TcpListenerComponent::GetComponentHealth() const noexcept {
    return m_health;
}

std::expected<void, std::string> TcpListenerComponent::handleListenerEvent(
    uint32_t events) {
    if (events & (EPOLLERR | EPOLLHUP)) {
        m_health = ComponentHealth::kDegraded;
        return std::unexpected("Error on listener socket");
    }

    if (events & EPOLLIN) {
        return acceptConnections();
    }

    return {};
}

std::expected<void, std::string> TcpListenerComponent::acceptConnections() {
    while (true) {
        auto client_result = m_listener->accept_connection();
        if (!client_result) {
            return std::unexpected(client_result.error());
        }

        int client_fd = client_result.value();
        if (client_fd == Socket::kInvalidFd) {
            break;
        }

        if (m_clients.size() >= kMaxClients) {
            close(client_fd);
            continue;
        }

        auto add_status = m_event_loop.AddFd(
            client_fd, EPOLLIN | EPOLLET, [this](int fd, uint32_t events) {
                return handleClientEvent(fd, events);
            });

        if (!add_status) {
            close(client_fd);
            continue;
        }

        m_clients[client_fd] = true;
        m_stats.increment_total();
        m_stats.increment_current();
    }

    return {};
}

std::expected<void, std::string> TcpListenerComponent::handleClientEvent(
    int fd, uint32_t events) {
    auto close_client = [this](int* fd_ptr) {
        if (fd_ptr && *fd_ptr == -1) {
            return;
        }

        closeClient(*fd_ptr);
    };

    std::unique_ptr<int, decltype(close_client)> client_close_guard(
        &fd, close_client);

    if (events & EPOLLERR) {
        int error = 0;
        socklen_t errlen = sizeof(error);

        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &errlen) == 0) {
            return std::unexpected(std::strerror(error));
        }
        return std::unexpected("");
    }

    if (events & EPOLLHUP) {
        return {};
    }

    if (events & EPOLLIN) {
        char buffer[kBufferSize];

        while (true) {
            ssize_t read_size = recv(fd, buffer, sizeof(buffer), 0);

            if (read_size < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                return std::unexpected(std::strerror(errno));
            }

            if (read_size == 0) {
                return {};
            }

            std::string message(buffer, static_cast<size_t>(read_size));
            std::string response = m_command_processor.process(message);

            size_t total_send = 0;
            while (total_send < response.size()) {
                ssize_t send_count =
                    send(fd, response.c_str() + total_send,
                         response.size() - total_send, MSG_NOSIGNAL);
                if (send_count < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    return std::unexpected(std::strerror(errno));
                }
                total_send += static_cast<size_t>(send_count);
            }
        }
    }

    [[maybe_unused]] auto _ = client_close_guard.release();
    return {};
}

void TcpListenerComponent::closeClient(int fd) {
    m_clients.erase(fd);
    [[maybe_unused]] auto _ = m_event_loop.RemoveFd(fd);
    close(fd);
    m_stats.decrement_current();
}

}  // namespace async_server
