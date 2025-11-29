#pragma once

// stlcpp
#include <concepts>
#include <expected>
#include <string>
#include <string_view>

namespace async_server {

enum class ComponentHealth { kOk, kDegraded, kUnhealthy };

constexpr std::string_view ComponentHealthToString(
    ComponentHealth health) noexcept {
    switch (health) {
        case ComponentHealth::kOk:
            return "ok";
        case ComponentHealth::kDegraded:
            return "degraded";
        case ComponentHealth::kUnhealthy:
            return "unhealthy";
        default:
            return "unknown";
    }
}

class ComponentBase {
   public:
    virtual ~ComponentBase() = default;

    virtual std::expected<void, std::string> Start() = 0;
    virtual std::expected<void, std::string> Stop() = 0;
    virtual ComponentHealth GetComponentHealth() const noexcept = 0;
};

template <typename ComponentType>
concept ComponentLike = requires(ComponentType component) {
    requires(std::derived_from<ComponentType, ComponentBase>);
    { component.kName } -> std::same_as<std::string_view>;
};

}  // namespace async_server
