#pragma once

namespace async_server {

class Config {
public:
    Config()
        : m_tcp_port(kDefaultTcpPort), m_udp_port(kDefaultUdpPort) {};

    Config(int tcp_port, int udp_port) 
        : m_tcp_port(tcp_port), m_udp_port(udp_port) {}

public:
    static Config FromEnv();
    
    int GetTcpPort() const { return m_tcp_port; }
    int GetUdpPort() const { return m_udp_port; }

private:
    int m_tcp_port;
    int m_udp_port;
    
    static constexpr int kDefaultTcpPort = 8080;
    static constexpr int kDefaultUdpPort = 8040;
};

}
