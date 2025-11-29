#include "component_manager.hpp"

// stlcpp
#include <atomic>
#include <event_loop/event_loop.hpp>
#include <expected>
#include <iostream>
#include <ranges>
#include <string>

#include "component/component_base.hpp"

namespace async_server {

ComponentManager::~ComponentManager() { [[maybe_unused]] auto _ = Stop(); }

bool ComponentManager::AddComponent(std::unique_ptr<ComponentBase> component) {
    if (!component) {
        return false;
    }

    if (m_running.load(std::memory_order::acquire)) {
        return false;
    }

    m_components.push_back(std::move(component));
    return true;
}

std::expected<void, std::string> ComponentManager::Start() {
    auto start_status = m_event_loop.Start();
    if (!start_status) {
        return std::unexpected(start_status.error());
    }

    if (ComponentHealth component_health = m_event_loop.GetComponentHealth();
        component_health != ComponentHealth::kOk) {
        return std::unexpected<std::string>(
            std::string(ComponentHealthToString(component_health)));
    }

    for (auto& component : m_components) {
        if (auto start_status = component->Start(); !start_status) {
            return std::unexpected(start_status.error());
        }

        if (ComponentHealth component_health = component->GetComponentHealth();
            component_health != ComponentHealth::kOk) {
            return std::unexpected<std::string>(
                std::string(ComponentHealthToString(component_health)));
        }
    }

    m_running.store(true, std::memory_order_release);
    return {};
}

std::expected<void, std::string> ComponentManager::Stop() {
    m_running.store(false, std::memory_order_relaxed);

    for (auto& comoponent : m_components | std::views::reverse) {
        if (auto stop_status = comoponent->Stop(); !stop_status) {
            return stop_status;
        }
    }

    return m_event_loop.Stop();
}

ComponentManager::RunStatus ComponentManager::Run() {
    if (!m_running.load(std::memory_order_acquire)) {
        return RunStatus::kNotStarted;
    }

    if (EventLoop::RunStatus event_loop_status = m_event_loop.Run();
        event_loop_status != EventLoop::RunStatus::kOk) {
        return RunStatus::kEventLoopError;
    }

    return RunStatus::kOk;
}

ComponentHealth ComponentManager::GetComponentHealth() const noexcept {
    if (auto health_status = m_event_loop.GetComponentHealth();
        health_status != ComponentHealth::kOk) {
        return health_status;
    }

    for (const auto& component : m_components) {
        if (auto health_status = component->GetComponentHealth();
            health_status != ComponentHealth::kOk) {
            return health_status;
        }
    }

    return ComponentHealth::kOk;
}

}  // namespace async_server
