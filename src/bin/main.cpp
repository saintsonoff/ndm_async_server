// stlcpp
#include <atomic>
#include <iostream>
#include <memory>

// stlc
#include <csignal>

// unix
#include <sys/signalfd.h>
#include <unistd.h>

// self
#include <command/command.hpp>
#include <component_manager/component_manager_builder.hpp>
#include <config/config.hpp>

#include "component_manager/component_manager.hpp"

int main() {
    async_server::Statistics stats;
    std::atomic<bool> shutdown_flag{false};

    auto manager_result =
        async_server::ComponentManagerBuilder(async_server::Config::FromEnv(),
                                              stats, shutdown_flag)
            .Build();

    if (!manager_result) {
        std::cerr << "ComponentManager build failed: " << manager_result.error()
                  << "\n";
        return 1;
    }

    auto& manager = *manager_result;

    if (auto start_status = manager->Start(); !start_status) {
        std::cerr << "ComponentManager start error: " << start_status.error()
                  << "\n";
        return 1;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        std::cerr << "Failed to block signals\n";
        return 1;
    }

    int signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd == -1) {
        std::cerr << "Failed to create signalfd\n";
        return 1;
    }

    auto close_fd = [](int* fd) {
        if (fd && *fd != -1) {
            close(*fd);
        }
    };
    std::unique_ptr<int, decltype(close_fd)> signal_fd_guard(&signal_fd,
                                                             close_fd);
    if (auto add_status = manager->GetEventLoop().AddFd(
            signal_fd, EPOLLIN,
            [&manager](int, uint32_t) {
                [[maybe_unused]] auto _ = manager->Stop();
                return std::expected<void, std::string>{};
            });
        !add_status) {
        std::cerr << "Failed to register signal FD: " << add_status.error()
                  << "\n";
        return 1;
    }

    if (manager->GetComponentHealth() != async_server::ComponentHealth::kOk) {
        std::cerr << "ComponentManager is not healthy\n";
        return 1;
    }

    if (auto run_status = manager->Run();
        run_status != async_server::ComponentManager::RunStatus::kOk) {
        std::cerr << "ComponentManager run error: "
                  << async_server::ComponentManager::RunStatusToString(
                         run_status)
                  << "\n";
        return 1;
    }

    if (auto stop_status = manager->Stop(); !stop_status) {
        std::cerr << "ComponentManager stop error: " << stop_status.error()
                  << "\n";
        return 1;
    }

    return 0;
}