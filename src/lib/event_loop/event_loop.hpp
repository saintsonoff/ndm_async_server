#pragma once

// stlcpp
#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

// unix
#include <sys/epoll.h>

// self
#include <component/component_base.hpp>

namespace async_server {

class EventLoop : public ComponentBase {
   public:
    static constexpr std::string_view kName = "EventLoop";

   public:
    enum class RunStatus {
        kOk,
        kEpollError,
        kExecError,
    };

    using EventCallback = std::function<std::expected<void, std::string>(
        int fd, uint32_t events)>;

   public:
    static constexpr int kMaxEvents = 128;

    EventLoop();
    ~EventLoop() override;

    std::expected<void, std::string> Start() override;
    std::expected<void, std::string> Stop() override;
    ComponentHealth GetComponentHealth() const noexcept override;

    std::expected<void, std::string> AddFd(int fd, uint32_t events,
                                           EventCallback callback);
    std::expected<void, std::string> RemoveFd(int fd);
    std::expected<void, std::string> ModifyFd(int fd, uint32_t events);
    RunStatus Run();
    bool IsRunning() const noexcept {
        return m_running.load(std::memory_order_acquire);
    }

   private:
    int m_epoll_fd;
    std::atomic<bool> m_running;
    std::unordered_map<int, EventCallback> m_callbacks;
    ComponentHealth m_health;
};

}  // namespace async_server
