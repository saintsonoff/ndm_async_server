// stlcpp
#include <iostream>

// stlc
#include <csignal>

// unix
#include <sys/signalfd.h>
#include <unistd.h>

// self
#include <config/config.hpp>
#include <memory>
#include <server/server.hpp>

int main() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        std::cerr << "Failed to block signals\n";
        return 1;
    }

    auto close_signal_fd_f = [](int* fd_ptr){
        close(*fd_ptr);
    };

    int signal_fd_value = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    std::unique_ptr<int, decltype(close_signal_fd_f)> 
        signal_fd(&signal_fd_value, close_signal_fd_f);

    if (*signal_fd == -1) {
        std::cerr << "Failed to create signalfd\n";
        return 1;
    }

    async_server::Server server(
        async_server::Config::FromEnv()
    );

    if (!server.Initialize()) {
        return 1;
    }

    server.AddSignalFd(*signal_fd);
    server.Run();

    return 0;
}