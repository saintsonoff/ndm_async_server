#pragma once

// stlcpp
#include <memory>
#include <string_view>

// unix
#include <sys/epoll.h>

// self
#include <command/command.hpp>
#include <component/component_base.hpp>
#include <event_loop/event_loop.hpp>
#include <socket/socket.hpp>

namespace async_server {

class UdpListenerComponent : public ComponentBase {
    static constexpr std::string_view kName = "UdpListenerComponent";

   public:
    UdpListenerComponent(EventLoop& event_loop,
                         std::unique_ptr<UdpSocket> listener, Statistics& stats,
                         std::atomic<bool>& shutdown_flag);

    ~UdpListenerComponent() override;

    std::expected<void, std::string> Start() override;
    std::expected<void, std::string> Stop() override;
    ComponentHealth GetComponentHealth() const noexcept override;

   private:
    std::expected<void, std::string> handleUdpEvent(int fd, uint32_t events);

    EventLoop& m_event_loop;
    std::unique_ptr<UdpSocket> m_listener;
    Statistics& m_stats;
    CommandProcessor m_command_processor;
    ComponentHealth m_health;

    static constexpr size_t kBufferSize = 8192;
};

}  // namespace async_server
