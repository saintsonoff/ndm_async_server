#pragma once

// stlcpp
#include <memory>
#include <optional>
#include <string>

// self
#include "server.hpp"
#include <config/config.hpp>


namespace async_server {


class ServerBuilder {
public:
    explicit ServerBuilder(Config config);
    
    ServerBuilder& WithSignalFd(int signal_fd);
    
    std::expected<std::unique_ptr<Server>, std::string> Build();
    
private:
    Config m_config;
    int m_signal_fd = -1;
};


} // namespace async_server
