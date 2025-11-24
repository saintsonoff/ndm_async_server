// stlcpp
#include <iostream>

//stlc
#include <csignal>

// unix
#include <sys/signalfd.h>
#include <unistd.h>

// self
#include <server/server.hpp>
#include <config/config.hpp>

int main() {
    // Блокируем сигналы для обработки через signalfd
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        std::cerr << "Failed to block signals\n";
        return 1;
    }
    
    // Создаём signalfd для получения сигналов через epoll
    int signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd == -1) {
        std::cerr << "Failed to create signalfd\n";
        return 1;
    }
    
    async_server::Config config = async_server::Config::load();
    async_server::Server server(std::move(config));
    
    if (!server.initialize()) {
        close(signal_fd);
        return 1;
    }
    
    server.add_signal_fd(signal_fd);
    server.run();
    
    close(signal_fd);
    return 0;
}