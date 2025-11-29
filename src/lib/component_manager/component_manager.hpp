#pragma once

// stlcpp
#include <atomic>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// self
#include <component/component_base.hpp>
#include <event_loop/event_loop.hpp>

namespace async_server {

class ComponentManager : ComponentBase {
   public:
    enum class RunStatus {
        kNotStarted,
        kEventLoopError,
        kOk,
    };

    static constexpr std::string_view RunStatusToString(RunStatus status) {
        switch (status) {
            case RunStatus::kNotStarted:
                return "not started error";
            case RunStatus::kEventLoopError:
                return "event loop error";
            case RunStatus::kOk:
                return "successfully";
            default:
                return "status not define";
        }
    }

   public:
    ComponentManager() = default;
    ~ComponentManager() override;

    ComponentManager(const ComponentManager&) = delete;
    ComponentManager& operator=(const ComponentManager&) = delete;
    ComponentManager(ComponentManager&&) = delete;
    ComponentManager& operator=(ComponentManager&&) = delete;

    bool AddComponent(std::unique_ptr<ComponentBase> component);

    EventLoop& GetEventLoop() { return m_event_loop; }

    RunStatus Run();
    std::expected<void, std::string> Stop() override;
    std::expected<void, std::string> Start() override;
    ComponentHealth GetComponentHealth() const noexcept override;

   private:
    EventLoop m_event_loop;
    std::vector<std::unique_ptr<ComponentBase>> m_components;
    std::atomic<bool> m_running = false;
};

}  // namespace async_server
