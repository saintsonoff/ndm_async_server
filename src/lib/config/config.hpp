#pragma once

namespace async_server {

class Config final {
   public:
    constexpr Config()
        : m_tcp_port(kDefaultTcpPort), m_udp_port(kDefaultUdpPort) {}

    constexpr Config(int tcp_port, int udp_port)
        : m_tcp_port(tcp_port), m_udp_port(udp_port) {}

   public:
    static Config FromEnv();

    constexpr int GetTcpPort() const noexcept { return m_tcp_port; }
    constexpr int GetUdpPort() const noexcept { return m_udp_port; }

   private:
    int m_tcp_port;
    int m_udp_port;

    static constexpr int kDefaultTcpPort = 8080;
    static constexpr int kDefaultUdpPort = 8040;
};

}  // namespace async_server
