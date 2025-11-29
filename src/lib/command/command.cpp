#include "command.hpp"

// stlcpp
#include <iomanip>
#include <sstream>

// stlc
#include <ctime>

namespace async_server {

std::string CommandProcessor::process(const std::string& message) {
    if (message.empty()) {
        return message;
    }

    if (message[0] != '/') {
        return message;
    }

    std::string cmd = message;
    if (cmd.back() == '\n') {
        cmd.pop_back();
    }

    if (cmd == "/time") {
        return get_time() + "\n";
    } else if (cmd == "/stats") {
        return get_stats() + "\n";
    } else if (cmd == "/shutdown") {
        should_shutdown_.store(true, std::memory_order_relaxed);
        return "Server shutting down\n";
    }

    return message;
}

std::string CommandProcessor::get_time() const {
    time_t now = std::time(nullptr);
    tm* local = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string CommandProcessor::get_stats() const {
    std::ostringstream oss;
    oss << "Total: " << stats_.get_total() << "\t"
        << "Current: " << stats_.get_current();
    return oss.str();
}

}  // namespace async_server
