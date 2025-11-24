#pragma once

namespace async_server {

class Config {
public:
    static Config load();
    
    int get_tcp_port() const { return tcp_port_; }
    int get_udp_port() const { return udp_port_; }
    
private:
    Config(int tcp_port, int udp_port) 
        : tcp_port_(tcp_port), udp_port_(udp_port) {}
    
    int tcp_port_;
    int udp_port_;
    
    static constexpr int DEFAULT_TCP_PORT = 8080;
    static constexpr int DEFAULT_UDP_PORT = 8040;
};

}
