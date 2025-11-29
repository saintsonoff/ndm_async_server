#include "event_loop.hpp"

// stlc
#include <cstring>

// stlcpp
#include <expected>
#include <iostream>
#include <string>

// unix
#include <sys/eventfd.h>
#include <unistd.h>

// self
#include <component/component_base.hpp>

namespace async_server {

EventLoop::EventLoop()
    : m_epoll_fd(-1),
      m_wakeup_fd(-1),
      m_running(false),
      m_health(ComponentHealth::kUnhealthy) {}

EventLoop::~EventLoop() { [[maybe_unused]] auto _ = Stop(); }

std::expected<void, std::string> EventLoop::Start() {
    if (m_epoll_fd != -1) {
        return std::unexpected("Epoll was inited");
    }

    m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epoll_fd == -1) {
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected(std::strerror(errno));
    }

    m_wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_wakeup_fd == -1) {
        close(m_epoll_fd);
        m_epoll_fd = -1;
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected(std::strerror(errno));
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = m_wakeup_fd;
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_wakeup_fd, &ev) == -1) {
        close(m_wakeup_fd);
        close(m_epoll_fd);
        m_wakeup_fd = -1;
        m_epoll_fd = -1;
        m_health = ComponentHealth::kUnhealthy;
        return std::unexpected(std::strerror(errno));
    }

    m_running.store(true, std::memory_order_release);
    m_health = ComponentHealth::kOk;

    return {};
}

std::expected<void, std::string> EventLoop::Stop() {
    m_running.store(false, std::memory_order_release);

    if (m_wakeup_fd != -1) {
        uint64_t val = 1;
        [[maybe_unused]] ssize_t _ = write(m_wakeup_fd, &val, sizeof(val));
    }

    if (m_wakeup_fd != -1) {
        close(m_wakeup_fd);
        m_wakeup_fd = -1;
    }

    if (m_epoll_fd != -1) {
        close(m_epoll_fd);
        m_epoll_fd = -1;
    }

    m_callbacks.clear();
    m_health = ComponentHealth::kUnhealthy;

    return {};
}

ComponentHealth EventLoop::GetComponentHealth() const noexcept {
    return m_health;
}

std::expected<void, std::string> EventLoop::AddFd(int fd, uint32_t events,
                                                  EventCallback callback) {
    if (m_epoll_fd == -1 || !callback) {
        return std::unexpected("epoll not started");
    }

    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        return std::unexpected(std::strerror(errno));
    }

    m_callbacks[fd] = std::move(callback);
    return {};
}

std::expected<void, std::string> EventLoop::RemoveFd(int fd) {
    if (m_epoll_fd == -1) {
        m_callbacks.erase(fd);
        return {};
    }

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        if (errno != EBADF && errno != ENOENT) {
            return std::unexpected(std::strerror(errno));
        }
    }

    m_callbacks.erase(fd);
    return {};
}

std::expected<void, std::string> EventLoop::ModifyFd(int fd, uint32_t events) {
    if (m_epoll_fd == -1) {
        return std::unexpected("epoll not started");
    }

    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        return std::unexpected(std::strerror(errno));
    }

    return {};
}

EventLoop::RunStatus EventLoop::Run() {
    if (m_epoll_fd == -1) {
        return EventLoop::RunStatus::kEpollError;
    }

    epoll_event events[kMaxEvents];

    while (m_running.load(std::memory_order_acquire)) {
        int nfds =
            epoll_wait(m_epoll_fd, events, kMaxEvents, 100);  // 100ms timeout

        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }

            m_health = ComponentHealth::kDegraded;
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == m_wakeup_fd) {
                continue;
            }

            auto it = m_callbacks.find(fd);
            if (it != m_callbacks.end()) {
                try {
                    auto exec_status = it->second(fd, ev);
                    if (!exec_status) {
                        return RunStatus::kExecError;
                    }
                } catch (const std::exception& e) {
                    m_health = ComponentHealth::kDegraded;
                }
            }
        }
    }

    return RunStatus::kOk;
}

}  // namespace async_server
