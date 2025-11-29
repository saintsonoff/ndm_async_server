#pragma once

// stlcpp
#include <memory>
#include <string_view>
#include <unordered_map>

// unix
#include <sys/epoll.h>

// self
#include <command/command.hpp>
#include <component/component_base.hpp>
#include <event_loop/event_loop.hpp>
#include <socket/socket.hpp>

namespace async_server {

class TcpListenerComponent : public ComponentBase {
    static constexpr std::string_view kName = "TcpListenerComponent";

   public:
    TcpListenerComponent(EventLoop& event_loop,
                         std::unique_ptr<TcpSocket> listener, Statistics& stats,
                         std::atomic<bool>& shutdown_flag);

    ~TcpListenerComponent() override;

    std::expected<void, std::string> Start() override;
    std::expected<void, std::string> Stop() override;
    ComponentHealth GetComponentHealth() const noexcept override;

    size_t GetActiveClientsCount() const noexcept { return m_clients.size(); }

   private:
    std::expected<void, std::string> handleListenerEvent(uint32_t events);
    std::expected<void, std::string> handleClientEvent(int fd, uint32_t events);
    std::expected<void, std::string> acceptConnections();
    void closeClient(int fd);

    EventLoop& m_event_loop;
    std::unique_ptr<TcpSocket> m_listener;
    Statistics& m_stats;
    CommandProcessor m_command_processor;
    std::unordered_map<int, bool> m_clients;
    ComponentHealth m_health;

    static constexpr size_t kBufferSize = 4096;
};

}  // namespace async_server
