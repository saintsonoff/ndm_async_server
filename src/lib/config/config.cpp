#include "config.hpp"

// stlc
#include <cstdlib>
#include <cstdio>

namespace async_server {

Config Config::load() {
    int tcp_port = DEFAULT_TCP_PORT;
    int udp_port = DEFAULT_UDP_PORT;
    
    if (const char* env_tcp = std::getenv("TCP_PORT")) {
        int port = 0;
        if (std::sscanf(env_tcp, "%d", &port) == 1 && port > 0 && port <= 65535) {
            tcp_port = port;
        }
    }
    
    if (const char* env_udp = std::getenv("UDP_PORT")) {
        int port = 0;
        if (std::sscanf(env_udp, "%d", &port) == 1 && port > 0 && port <= 65535) {
            udp_port = port;
        }
    }
    
    return Config(tcp_port, udp_port);
}

}
