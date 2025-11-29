#pragma once

// stlcpp
#include <expected>
#include <memory>
#include <optional>
#include <string>

// self
#include <command/command.hpp>
#include <config/config.hpp>

#include "component_manager.hpp"

namespace async_server {

class ComponentManagerBuilder {
   public:
    explicit ComponentManagerBuilder(Config config, Statistics& stats,
                                     std::atomic<bool>& shutdown_flag);

    std::expected<std::unique_ptr<ComponentManager>, std::string> Build();

   private:
    Config m_config;
    Statistics& m_stats;
    std::atomic<bool>& m_shutdown_flag;
};

}  // namespace async_server
