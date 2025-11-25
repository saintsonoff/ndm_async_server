// stlcpp
#include <iostream>
#include <memory>

// stlc
#include <csignal>

// unix
#include <sys/signalfd.h>
#include <unistd.h>

// self
#include <config/config.hpp>
#include <server/server_builder.hpp>

int main() {
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
    std::unique_ptr<int, decltype(close_fd)> signal_fd_guard(&signal_fd, close_fd);

    auto server_result = async_server::ServerBuilder(async_server::Config::FromEnv())
        .WithSignalFd(signal_fd)
        .Build();
    
    if (!server_result) {
        std::cerr << "Server build failed: " << server_result.error() << "\n";
        return 1;
    }

    auto server = std::move(*server_result);
    server->Run();
    
    return 0;
}