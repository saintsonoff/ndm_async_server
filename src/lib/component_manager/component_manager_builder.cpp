#include "component_manager_builder.hpp"

// stlcpp
#include <memory>

// self
#include <socket/socket.hpp>
#include <tcp_listener/tcp_listener_component.hpp>
#include <udp_listener/udp_listener_component.hpp>

// unix
#include <sys/epoll.h>
#include <unistd.h>

namespace async_server {

ComponentManagerBuilder::ComponentManagerBuilder(
    Config config, Statistics& stats, std::atomic<bool>& shutdown_flag)
    : m_config(std::move(config)),
      m_stats(stats),
      m_shutdown_flag(shutdown_flag) {}

std::expected<std::unique_ptr<ComponentManager>, std::string>
ComponentManagerBuilder::Build() {
    auto manager = std::make_unique<ComponentManager>();

    auto tcp_result = TcpSocket::create(m_config.GetTcpPort());
    if (!tcp_result) {
        return std::unexpected(tcp_result.error());
    }
    auto tcp_socket = std::make_unique<TcpSocket>(std::move(*tcp_result));

    auto tcp_component = std::make_unique<TcpListenerComponent>(
        manager->GetEventLoop(), std::move(tcp_socket), m_stats,
        m_shutdown_flag);

    manager->AddComponent(std::move(tcp_component));

    auto udp_result = UdpSocket::create(m_config.GetUdpPort());
    if (!udp_result) {
        return std::unexpected(udp_result.error());
    }
    auto udp_socket = std::make_unique<UdpSocket>(std::move(*udp_result));

    auto udp_component = std::make_unique<UdpListenerComponent>(
        manager->GetEventLoop(), std::move(udp_socket), m_stats,
        m_shutdown_flag);
    manager->AddComponent(std::move(udp_component));

    return manager;
}

}  // namespace async_server
